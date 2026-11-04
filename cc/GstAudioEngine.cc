// GstAudioEngine.cc
#include "GstAudioEngine.h"
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/base/gstbasesink.h>
#include <gst/audio/gstaudiobasesink.h>
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cstring>

namespace {
constexpr int kChannels = 2;  // engine works in interleaved stereo

std::string rawCaps(int rate) {
    return "audio/x-raw,format=F32LE,layout=interleaved,channels=" +
        std::to_string(kChannels) + ",rate=" + std::to_string(rate);
}

// Print errors and warnings from any thread as soon as they're posted.
GstBusSyncReply busSyncHandler(GstBus*, GstMessage* msg, gpointer) {
    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR || GST_MESSAGE_TYPE(msg) == GST_MESSAGE_WARNING) {
        GError* err = nullptr;
        gchar* debug = nullptr;
        bool isError = GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR;
        if (isError) {
            gst_message_parse_error(msg, &err, &debug);
        } else {
            gst_message_parse_warning(msg, &err, &debug);
        }
        printf("   ---GstAudioEngine::%s from %s: %s\n", isError ? "ERROR" : "WARNING",
            GST_OBJECT_NAME(GST_MESSAGE_SRC(msg)), err ? err->message : "?");
        g_clear_error(&err);
        g_free(debug);
    }
    return GST_BUS_PASS;
}

// audiomixer flags its output GAP ("no data here") whenever no voice is
// playing. Audio sinks like pulsesink don't feed GAP buffers to the sound
// server, so its stream runs dry between hits, underflows, and loses the start
// of the next hit while it resyncs. Turn GAP buffers back into real silence so
// the device gets a continuous stream.
GstPadProbeReturn clearGapProbe(GstPad*, GstPadProbeInfo* info, gpointer userData) {
    auto* meter = static_cast<GstAudioEngine::OutputMeter*>(userData);
    GstBuffer* buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    if (buffer == nullptr) {
        return GST_PAD_PROBE_OK;
    }
    if (GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_GAP)) {
        buffer = gst_buffer_make_writable(buffer);
        GstMapInfo map;
        if (gst_buffer_map(buffer, &map, GST_MAP_WRITE)) {
            std::memset(map.data, 0, map.size);  // F32 zero == silence
            gst_buffer_unmap(buffer, &map);
        }
        GST_BUFFER_FLAG_UNSET(buffer, GST_BUFFER_FLAG_GAP);
        GST_PAD_PROBE_INFO_DATA(info) = buffer;
        meter->decay();
        return GST_PAD_PROBE_OK;
    }
    // Real audio: measure its peak (shown in the window and at shutdown),
    // then soft-limit it. Overlapping notes add up past 1.0, which hard-clips
    // (distorts) at the sound card. Below 0.8 the signal is untouched; above
    // it, it bends smoothly toward 1.0 instead of being chopped off.
    buffer = gst_buffer_make_writable(buffer);
    GST_PAD_PROBE_INFO_DATA(info) = buffer;
    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_READWRITE)) {
        float* samples = reinterpret_cast<float*>(map.data);
        float peak = 0.0f;
        const float knee = 0.8f;
        for (size_t i = 0; i < map.size / sizeof(float); ++i) {
            float v = samples[i] < 0 ? -samples[i] : samples[i];
            if (v > peak) peak = v;
            if (v > knee) {
                float limited = knee + (1.0f - knee) * std::tanh((v - knee) / (1.0f - knee));
                samples[i] = samples[i] < 0 ? -limited : limited;
            }
        }
        gst_buffer_unmap(buffer, &map);
        meter->record(peak);   // pre-limiter peak: >1.0 means notes are stacking up
    }
    return GST_PAD_PROBE_OK;
}

constexpr int64_t kNoCut = INT64_MAX;
} // namespace

GstAudioEngine::GstAudioEngine(const Config& cfg) : config(cfg) {
    gst_init(nullptr, nullptr);  // safe to call more than once
}

GstAudioEngine::~GstAudioEngine() {
    shutdown();
}

bool GstAudioEngine::start() {
    if (running) {
        return true;
    }
    const std::string caps = rawCaps(config.sampleRate);
    const guint64 mixerLatencyNs = static_cast<guint64>(config.mixerLatencyMs) * GST_MSECOND;

    // Build the pipeline as a gst-launch style string: easy to read, and the
    // sink can be swapped from the YAML config without touching code.
    std::string description =
        "audiomixer name=mix latency=" + std::to_string(mixerLatencyNs) +
        // Pin the mixer's output format. Unpinned, audiomixer negotiated it
        // at startup and sometimes picked 44.1 kHz / F64, which made every
        // voice get resampled inside the mixer. Conversion for the sound
        // card happens once, after the mix.
        " ! " + caps + " ! audioconvert ! audioresample ! " + config.sink + " ";
    for (int i = 0; i < config.voices; ++i) {
        description += "appsrc name=voice" + std::to_string(i) +
            " is-live=true format=time min-latency=0 max-bytes=0 caps=\"" + caps + "\" ! mix. ";
    }
    if (config.verbose) {
        printf("   GstAudioEngine::start::Pipeline: %s\n", description.c_str());
    }

    GError* error = nullptr;
    pipeline = gst_parse_launch(description.c_str(), &error);
    if (pipeline == nullptr || error != nullptr) {
        printf("   ---GstAudioEngine::start::Failed to build pipeline: %s\n", error ? error->message : "?");
        g_clear_error(&error);
        if (pipeline) {
            gst_object_unref(pipeline);
            pipeline = nullptr;
        }
        return false;
    }
    GstBus* bus = gst_element_get_bus(pipeline);
    gst_bus_set_sync_handler(bus, busSyncHandler, nullptr, nullptr);
    gst_object_unref(bus);

    GstElement* mixer = gst_bin_get_by_name(GST_BIN(pipeline), "mix");
    GstPad* mixerSrc = gst_element_get_static_pad(mixer, "src");
    gst_pad_add_probe(mixerSrc, GST_PAD_PROBE_TYPE_BUFFER, clearGapProbe, &outputMeter, nullptr);
    gst_object_unref(mixerSrc);
    gst_object_unref(mixer);

    voices.resize(config.voices);
    for (int i = 0; i < config.voices; ++i) {
        std::string name = "voice" + std::to_string(i);
        voices[i].src = GST_APP_SRC(gst_bin_get_by_name(GST_BIN(pipeline), name.c_str()));
    }

    // Hand every voice its first 20 ms of (silent) stream so caps and
    // segments flow and the mixer and sink start running before the first
    // note. From then on the feeder thread keeps every voice topped up.
    const int64_t chunkFrames = config.sampleRate / 100;
    for (auto& voice : voices) {
        pushChunk(voice, chunkFrames);
        pushChunk(voice, chunkFrames);
    }

    // Make the audio device the pipeline's clock.
    //
    // A live pipeline otherwise runs on GstSystemClock and the audio sink
    // has to keep correcting itself against it ("slaving"). Whether that
    // works depends on how the sound server reports time. With PipeWire +
    // Bluetooth the sink judged every buffer late and dropped it: the mixer
    // produced sound and the speakers stayed silent. With the sink's clock
    // as master there's nothing to correct: the scheduler, the mixer and
    // the speakers all run on the sound card's time.
    //
    // autoaudiosink only creates its real sink at READY, so go there first.
    gst_element_set_state(pipeline, GST_STATE_READY);
    GstClock* deviceClock = nullptr;
    if (config.useSinkClock) {
        GstIterator* sinks = gst_bin_iterate_recurse(GST_BIN(pipeline));
        GValue item = G_VALUE_INIT;
        while (deviceClock == nullptr && gst_iterator_next(sinks, &item) == GST_ITERATOR_OK) {
            GstElement* element = GST_ELEMENT(g_value_get_object(&item));
            if (GST_IS_AUDIO_BASE_SINK(element) && GST_AUDIO_BASE_SINK(element)->provided_clock) {
                deviceClock = GST_CLOCK(gst_object_ref(GST_AUDIO_BASE_SINK(element)->provided_clock));
            }
            g_value_reset(&item);
        }
        g_value_unset(&item);
        gst_iterator_free(sinks);
    }
    if (deviceClock != nullptr) {
        gst_pipeline_use_clock(GST_PIPELINE(pipeline), deviceClock);
        gst_object_unref(deviceClock);
    }

    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        printf("   ---GstAudioEngine::start::Pipeline refused to go to PLAYING.\n");
        return false;
    }
    gst_element_get_state(pipeline, nullptr, nullptr, 2 * GST_SECOND);

    // Hand the pipeline clock to StreamClock. From here on every TimePoint
    // in the program is pipeline running time.
    GstClock* clock = gst_pipeline_get_clock(GST_PIPELINE(pipeline));
    StreamClock::attach(clock, gst_element_get_base_time(pipeline));

    GstQuery* query = gst_query_new_latency();
    if (gst_element_query(pipeline, query)) {
        gboolean live = FALSE;
        GstClockTime minLatency = 0, maxLatency = 0;
        gst_query_parse_latency(query, &live, &minLatency, &maxLatency);
        pipelineLatency = Duration(static_cast<Duration::rep>(minLatency));
    }
    gst_query_unref(query);

    printf("   GstAudioEngine::start::Playing. Clock: %s, voices: %d, feed-ahead: %d ms, pipeline latency: %.1f ms\n",
        GST_OBJECT_NAME(clock), config.voices, config.feedAheadMs,
        std::chrono::duration<double, std::milli>(pipelineLatency).count());

    // Say which output device element is really in use. autoaudiosink picks
    // one at runtime (pulsesink, pipewiresink, alsasink, ...) and quietly
    // falls back to another if the first can't connect, which can leave you
    // "playing" into an output nobody hears.
    GstIterator* it = gst_bin_iterate_recurse(GST_BIN(pipeline));
    GValue item = G_VALUE_INIT;
    while (gst_iterator_next(it, &item) == GST_ITERATOR_OK) {
        GstElement* element = GST_ELEMENT(g_value_get_object(&item));
        if (GST_IS_BASE_SINK(element) && !GST_IS_BIN(element)) {
            GstElementFactory* factory = gst_element_get_factory(element);
            std::string device;
            if (g_object_class_find_property(G_OBJECT_GET_CLASS(element), "device")) {
                gchar* value = nullptr;
                g_object_get(element, "device", &value, nullptr);
                if (value) { device = value; g_free(value); }
            }
            const char* factoryName = factory ? GST_OBJECT_NAME(factory) : GST_OBJECT_NAME(element);
            printf("   GstAudioEngine::start::Audio output: %s%s%s\n", factoryName,
                device.empty() ? "" : " device=", device.c_str());
            if (std::strcmp(factoryName, "fakesink") == 0) {
                printf("   ---GstAudioEngine::start::NO AUDIO DEVICE: autoaudiosink couldn't open any real output and\n"
                       "      fell back to fakesink, which discards all audio. Check that PulseAudio/PipeWire is running\n"
                       "      (pactl info), or set audioMixer.gst_sink to e.g. \"alsasink device=hw:0\" or \"pulsesink\".\n");
            }
        }
        g_value_reset(&item);
    }
    g_value_unset(&item);
    gst_iterator_free(it);
    gst_object_unref(clock);
    running = true;
    feederRunning = true;
    feederThread = std::thread(&GstAudioEngine::feederLoop, this);
    return true;
}

void GstAudioEngine::shutdown() {
    if (pipeline == nullptr) {
        return;
    }
    feederRunning = false;
    if (feederThread.joinable()) {
        feederThread.join();
    }
    // Only file sinks need draining (wavenc rewrites its header at EOS). For
    // a live sound card, waiting for EOS meant sitting through up to 5 s of
    // queued audio after the window closed, so just stop.
    const bool drain = config.sink.find("filesink") != std::string::npos;
    if (running && drain) {
        // EOS on every voice -> audiomixer sends EOS downstream once all are
        // done -> file sinks (wavenc) can finalize their headers.
        {
            std::lock_guard<std::mutex> lock(voicesMutex);
            for (auto& voice : voices) {
                gst_app_src_end_of_stream(voice.src);
            }
        }
        GstBus* bus = gst_element_get_bus(pipeline);
        GstMessage* msg = gst_bus_timed_pop_filtered(bus, 5 * GST_SECOND,
            static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
        if (msg) {
            gst_message_unref(msg);
        }
        gst_object_unref(bus);
    }
    if (running) {
        printf("   GstAudioEngine::shutdown::Mixer output: %llu buffers with sound, loudest peak %.3f "
               "(0 = nothing ever reached the output device)\n",
            static_cast<unsigned long long>(outputMeter.audibleBuffers.load()), outputMeter.maxPeak.load());
        if (lateHits || stolenVoices) {
            printf("   GstAudioEngine::shutdown::Late hits (started a few ms late): %llu, "
                   "voices stolen (all %d busy, oldest note cut): %llu\n",
                static_cast<unsigned long long>(lateHits), config.voices,
                static_cast<unsigned long long>(stolenVoices));
        }
    }
    StreamClock::detach();
    gst_element_set_state(pipeline, GST_STATE_NULL);
    for (auto& voice : voices) {
        if (voice.src) {
            gst_object_unref(voice.src);
        }
    }
    voices.clear();
    gst_object_unref(pipeline);
    pipeline = nullptr;
    running = false;
}

std::vector<float> GstAudioEngine::decodeFile(const std::string& path) const {
    std::vector<float> out;
    // Fail fast with a clear message for the most common config mistake.
    if (FILE* probe = std::fopen(path.c_str(), "rb")) {
        std::fclose(probe);
    } else {
        printf("   ---GstAudioEngine::decodeFile::Cannot open file: %s\n", path.c_str());
        return out;
    }
    // decodebin picks the right demuxer/decoder; the capsfilter forces the
    // output into the engine's format, so resampling/channel mapping happen
    // here once at load time instead of on every hit.
    std::string description = "filesrc name=src ! decodebin ! audioconvert ! audioresample ! " +
        rawCaps(config.sampleRate) + " ! appsink name=sink sync=false";
    GError* error = nullptr;
    GstElement* decoder = gst_parse_launch(description.c_str(), &error);
    if (decoder == nullptr || error != nullptr) {
        printf("   ---GstAudioEngine::decodeFile::%s\n", error ? error->message : "pipeline error");
        g_clear_error(&error);
        if (decoder) {
            gst_object_unref(decoder);
        }
        return out;
    }
    GstElement* src = gst_bin_get_by_name(GST_BIN(decoder), "src");
    g_object_set(src, "location", path.c_str(), nullptr);  // set directly: no quoting issues
    gst_object_unref(src);
    GstAppSink* sink = GST_APP_SINK(gst_bin_get_by_name(GST_BIN(decoder), "sink"));
    GstBus* bus = gst_element_get_bus(decoder);

    gst_element_set_state(decoder, GST_STATE_PLAYING);
    // Don't use gst_app_sink_pull_sample(): if decoding fails (not an audio
    // file, unsupported format) appsink never sees EOS and that call blocks
    // forever. Poll with a timeout and watch the bus for errors instead.
    bool failed = false;
    const gint64 deadline = g_get_monotonic_time() + 30 * G_TIME_SPAN_SECOND;
    while (true) {
        GstSample* sample = gst_app_sink_try_pull_sample(sink, 100 * GST_MSECOND);
        if (sample != nullptr) {
            GstBuffer* buffer = gst_sample_get_buffer(sample);
            GstMapInfo map;
            if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
                const float* data = reinterpret_cast<const float*>(map.data);
                out.insert(out.end(), data, data + map.size / sizeof(float));
                gst_buffer_unmap(buffer, &map);
            }
            gst_sample_unref(sample);
            continue;
        }
        if (gst_app_sink_is_eos(sink)) {
            break;  // finished normally
        }
        if (GstMessage* msg = gst_bus_pop_filtered(bus, GST_MESSAGE_ERROR)) {
            GError* err = nullptr;
            gst_message_parse_error(msg, &err, nullptr);
            printf("   ---GstAudioEngine::decodeFile::%s: %s\n", path.c_str(), err ? err->message : "?");
            g_clear_error(&err);
            gst_message_unref(msg);
            failed = true;
            break;
        }
        if (g_get_monotonic_time() > deadline) {
            printf("   ---GstAudioEngine::decodeFile::Timed out decoding %s\n", path.c_str());
            failed = true;
            break;
        }
    }
    if (failed) {
        out.clear();
    }
    gst_object_unref(bus);
    gst_element_set_state(decoder, GST_STATE_NULL);
    gst_object_unref(sink);
    gst_object_unref(decoder);
    return out;
}

int64_t GstAudioEngine::Note::endFrame() const {
    const int64_t natural = startFrame + static_cast<int64_t>(samples->size() / kChannels);
    return natural < cutFrame ? natural : cutFrame;
}

int64_t GstAudioEngine::frameAt(TimePoint t) const {
    const int64_t ns = t.time_since_epoch().count();
    return ns <= 0 ? 0 : static_cast<int64_t>(gst_util_uint64_scale_round(ns, config.sampleRate, GST_SECOND));
}

// Build and push the next `chunkFrames` of this voice's stream: silence plus
// whatever part of its note(s) falls inside the chunk, at the exact frame.
void GstAudioEngine::pushChunk(Voice& voice, int64_t chunkFrames) {
    const int64_t start = voice.pushedFrame;
    const int64_t end = start + chunkFrames;
    const int64_t fadeFrames = config.sampleRate / 200;  // 5 ms
    GstBuffer* buffer = gst_buffer_new_allocate(nullptr, chunkFrames * kChannels * sizeof(float), nullptr);
    GstMapInfo map;
    gst_buffer_map(buffer, &map, GST_MAP_WRITE);
    float* out = reinterpret_cast<float*>(map.data);
    std::memset(out, 0, map.size);
    for (const Note& note : voice.notes) {
        const int64_t from = std::max(start, note.startFrame);
        const int64_t to = std::min(end, note.endFrame());
        const float* in = note.samples->data();
        for (int64_t f = from; f < to; ++f) {
            float gain = 1.0f;
            if (note.cutFrame != kNoCut && f >= note.cutFrame - fadeFrames) {
                gain = static_cast<float>(note.cutFrame - f) / static_cast<float>(fadeFrames);
            }
            const int64_t i = (f - note.startFrame) * kChannels;
            out[(f - start) * kChannels] += in[i] * gain;
            out[(f - start) * kChannels + 1] += in[i + 1] * gain;
        }
    }
    gst_buffer_unmap(buffer, &map);
    voice.notes.erase(std::remove_if(voice.notes.begin(), voice.notes.end(),
        [end](const Note& note) { return note.endFrame() <= end; }), voice.notes.end());

    const GstClockTime pts = gst_util_uint64_scale(start, GST_SECOND, config.sampleRate);
    GST_BUFFER_PTS(buffer) = pts;
    GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale(end, GST_SECOND, config.sampleRate) - pts;
    if (start == 0) {
        GST_BUFFER_FLAG_SET(buffer, GST_BUFFER_FLAG_DISCONT);
    }
    gst_app_src_push_buffer(voice.src, buffer);  // takes ownership; never blocks (max-bytes=0)
    voice.pushedFrame = end;
}

// Keeps every voice's stream filled to ~10 ms ahead of the clock. Only that
// much is committed in advance, so a note scheduled further ahead (the
// scheduler works `lookahead` ahead, 50 ms by default) always lands in audio
// that hasn't been pushed yet: it starts on its exact sample, and a stolen
// voice's old note can still be faded out.
void GstAudioEngine::feederLoop() {
    const int64_t chunkFrames = config.sampleRate / 100;  // 10 ms
    const Duration feedAhead = std::chrono::milliseconds(config.feedAheadMs);
    while (feederRunning) {
        const TimePoint now = StreamClock::now();
        const int64_t horizon = frameAt(now + feedAhead);
        {
            std::lock_guard<std::mutex> lock(voicesMutex);
            for (auto& voice : voices) {
                while (voice.pushedFrame < horizon) {
                    pushChunk(voice, chunkFrames);
                }
            }
        }
        StreamClock::sleepUntil(now + std::chrono::milliseconds(5));
    }
}

int GstAudioEngine::getActiveVoices() {
    std::lock_guard<std::mutex> lock(voicesMutex);
    const int64_t nowFrame = frameAt(StreamClock::now());
    int active = 0;
    for (const auto& voice : voices) {
        for (const auto& note : voice.notes) {
            if (note.endFrame() > nowFrame) {
                ++active;
                break;
            }
        }
    }
    return active;
}

bool GstAudioEngine::scheduleAt(const SampleData& samples, TimePoint when) {
    if (!running || !samples || samples->empty()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(voicesMutex);
    int64_t startFrame = frameAt(when);

    // A voice is free if everything on it has finished by the new start.
    Voice* chosen = nullptr;
    for (auto& voice : voices) {
        bool freeAtStart = std::all_of(voice.notes.begin(), voice.notes.end(),
            [startFrame](const Note& note) { return note.endFrame() <= startFrame; });
        if (freeAtStart) {
            chosen = &voice;
            break;
        }
    }
    if (chosen == nullptr) {
        // Steal the voice whose newest note started earliest, and fade that
        // note out. The fade has to sit in audio not yet pushed.
        int64_t oldest = INT64_MAX;
        for (auto& voice : voices) {
            int64_t newest = 0;
            for (const auto& note : voice.notes) {
                newest = std::max(newest, note.startFrame);
            }
            if (newest < oldest) {
                oldest = newest;
                chosen = &voice;
            }
        }
        const int64_t cut = std::max(startFrame, chosen->pushedFrame + config.sampleRate / 200);
        for (auto& note : chosen->notes) {
            note.cutFrame = std::min(note.cutFrame, cut);
        }
        ++stolenVoices;
    }
    if (startFrame < chosen->pushedFrame) {
        // Already committed past this time: start right after, a few ms late,
        // rather than losing the attack.
        startFrame = chosen->pushedFrame;
        ++lateHits;
    }
    Note note;
    note.samples = samples;
    note.startFrame = startFrame;
    chosen->notes.push_back(std::move(note));
    return true;
}
