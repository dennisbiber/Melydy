// GstAudioEngine.cc
#include "GstAudioEngine.h"
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <cstdio>
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

// Called when GStreamer is done with a buffer: drop our ref on the samples.
void releaseSamples(gpointer userData) {
    delete static_cast<GstAudioEngine::SampleData*>(userData);
}

GstBuffer* makeBuffer(const GstAudioEngine::SampleData& samples, int rate, TimePoint pts) {
    gsize bytes = samples->size() * sizeof(float);
    // Zero-copy: the buffer points straight at the player's decoded samples and
    // holds a shared_ptr copy until GStreamer releases it.
    GstBuffer* buffer = gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY,
        const_cast<float*>(samples->data()), bytes, 0, bytes,
        new GstAudioEngine::SampleData(samples), releaseSamples);
    guint64 frames = samples->size() / kChannels;
    GST_BUFFER_PTS(buffer) = static_cast<GstClockTime>(pts.time_since_epoch().count());
    GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale(frames, GST_SECOND, rate);
    // Each hit is its own island of audio on this voice. DISCONT tells
    // audiomixer to place it by its timestamp instead of assuming it follows
    // the previous buffer back-to-back.
    GST_BUFFER_FLAG_SET(buffer, GST_BUFFER_FLAG_DISCONT);
    return buffer;
}
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

    voices.resize(config.voices);
    for (int i = 0; i < config.voices; ++i) {
        std::string name = "voice" + std::to_string(i);
        voices[i].src = GST_APP_SRC(gst_bin_get_by_name(GST_BIN(pipeline), name.c_str()));
    }

    // Prime every voice with 10 ms of silence at PTS 0. This lets caps and
    // segments flow so the mixer and sink start running (and the audio
    // sink's clock starts ticking) before the first real hit.
    const size_t primeFrames = static_cast<size_t>(config.sampleRate) / 100;
    auto silence = std::make_shared<const std::vector<float>>(primeFrames * kChannels, 0.0f);
    for (auto& voice : voices) {
        gst_app_src_push_buffer(voice.src, makeBuffer(silence, config.sampleRate, TimePoint(Duration(0))));
        voice.busyUntil = TimePoint(Duration(gst_util_uint64_scale(primeFrames, GST_SECOND, config.sampleRate)));
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

    printf("   GstAudioEngine::start::Playing. Clock: %s, voices: %d, pipeline latency: %.1f ms\n",
        GST_OBJECT_NAME(clock), config.voices,
        std::chrono::duration<double, std::milli>(pipelineLatency).count());
    gst_object_unref(clock);
    running = true;
    return true;
}

void GstAudioEngine::shutdown() {
    if (pipeline == nullptr) {
        return;
    }
    if (running) {
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
        if (lateHits || droppedHits) {
            printf("   GstAudioEngine::shutdown::Late hits: %llu, dropped hits (no free voice): %llu\n",
                static_cast<unsigned long long>(lateHits), static_cast<unsigned long long>(droppedHits));
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

    gst_element_set_state(decoder, GST_STATE_PLAYING);
    while (GstSample* sample = gst_app_sink_pull_sample(sink)) {  // NULL at EOS or error
        GstBuffer* buffer = gst_sample_get_buffer(sample);
        GstMapInfo map;
        if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            const float* data = reinterpret_cast<const float*>(map.data);
            out.insert(out.end(), data, data + map.size / sizeof(float));
            gst_buffer_unmap(buffer, &map);
        }
        gst_sample_unref(sample);
    }

    GstBus* bus = gst_element_get_bus(decoder);
    if (GstMessage* msg = gst_bus_pop_filtered(bus, GST_MESSAGE_ERROR)) {
        GError* err = nullptr;
        gst_message_parse_error(msg, &err, nullptr);
        printf("   ---GstAudioEngine::decodeFile::%s: %s\n", path.c_str(), err ? err->message : "?");
        g_clear_error(&err);
        gst_message_unref(msg);
        out.clear();
    }
    gst_object_unref(bus);
    gst_element_set_state(decoder, GST_STATE_NULL);
    gst_object_unref(sink);
    gst_object_unref(decoder);
    return out;
}

bool GstAudioEngine::scheduleAt(const SampleData& samples, TimePoint when) {
    if (!running || !samples || samples->empty()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(voicesMutex);
    // Buffers on one appsrc must not overlap in time, so find a voice that
    // has finished its previous sound by `when`. This is the same job
    // SDL_mixer did when you passed channel -1 to Mix_PlayChannel.
    Voice* chosen = nullptr;
    for (auto& voice : voices) {
        if (voice.busyUntil <= when && (chosen == nullptr || voice.busyUntil < chosen->busyUntil)) {
            chosen = &voice;
        }
    }
    if (chosen == nullptr) {
        ++droppedHits;
        if (config.verbose) {
            printf("   ---GstAudioEngine::scheduleAt::No free voice; hit dropped.\n");
        }
        return false;
    }
    if (when < StreamClock::now()) {
        // Still pushed: audiomixer waits `mixerLatencyMs` for stragglers, and
        // anything later than that gets its start clipped.
        ++lateHits;
    }
    GstBuffer* buffer = makeBuffer(samples, config.sampleRate, when);
    chosen->busyUntil = when + Duration(static_cast<Duration::rep>(GST_BUFFER_DURATION(buffer)));
    gst_app_src_push_buffer(chosen->src, buffer);  // takes ownership
    return true;
}
