# Melydy (Built by exploring GPT as a coding assistant in 2022)
Drum Pad/Live Looper/Sequencer/Recording system/Synthesizer
Melydy is an open-source project with the end goal of providing a free solution for drum pad/Live Looper/AudioVisual Synthesis. The goal is also to have
this at production quality to integrate into any stage setup for live performance. 

Packages Need For Program:
```
libsdl2-dev
libsdl2-ttf-dev
libyaml-cpp-dev
fonts-freefont-ttf       # FreeSans.ttf for the window text
libgstreamer1.0-dev
libgstreamer-plugins-base1.0-dev
gstreamer1.0-plugins-base
gstreamer1.0-plugins-good
gstreamer1.0-alsa        # or gstreamer1.0-pulseaudio, for the audio sink
```

## Audio engine: GStreamer

Audio used to go through SDL_mixer. It now goes through a GStreamer pipeline
(`cc/GstAudioEngine.*`), and every time in the program is measured on the
pipeline's clock (`cc/StreamClock.*`):

```
appsrc voice0 ─┐
appsrc voice1 ─┤
     ...       ├─> audiomixer ─> audioconvert ─> audioresample ─> autoaudiosink
appsrc voiceN ─┘
```

How a sound is played now:
1. The beat grid is pipeline running time: division `k` is at `k * (60 / bpm / beatDivisions)`.
2. `MasterClock` runs each batch `scheduler_lookahead_ms` *before* its grid time and exposes that grid time as `getTickTime()`.
3. `AudioPlayer::playAt(tickTime)` pushes the decoded sample into a free `appsrc` voice with `PTS = tickTime`.
4. `audiomixer` places the buffer at the exact sample for that PTS, and the audio sink is the pipeline clock, so the scheduler and the sound card share one clock.

Timing bugs this fixes:
* **Loop drift.** Loops used to reschedule from the moment the thread woke up (`now + interval`), so every wake-up delay was added to every later repeat. They now reschedule from the previous tick (`tick + interval`).
* **Buffer quantization.** `Mix_PlayChannel` started sounds at the next mix buffer, up to ~43 ms late with a 2048-frame buffer. Timestamped buffers start on the exact sample.
* **Two clocks.** The scheduler ran on the CPU clock and playback on the sound card's crystal. Now there is only the pipeline clock.
* **Batch list changed mid-iteration.** Adding or removing a looper from inside a tick used to insert into the deque the scheduler was looping over (undefined behavior). Changes are now queued and applied between passes.
* Log file I/O moved off the scheduler thread, and looper ID tags are unique, so two loops recorded on the same division of different beats no longer share a batch.

Live key presses are still quantized to the next division, as before. Response time is now that division wait plus the lookahead plus the sink's latency (printed at startup).

### Threads
* **Main thread:** SDL. It pumps keyboard events (`KeyboardEvent::processEvent`) and draws the window (`GraphicPlayer::renderFrame`). SDL requires both to run on the thread that created the window. Pumping events from a separate keyboard thread caused the intermittent UI crashes, and the window was never drawn, which is why it showed up transparent.
* **Scheduler thread:** `MasterClock::executeScheduledBatches`.
* **GStreamer's own threads:** mixing and output.

Closing the window or pressing Ctrl-C shuts down cleanly: SDL turns Ctrl-C into a quit event.

Keypad keys are hold-to-arm. Hold `KP1`-`KP9` (loop length) plus `KP+` while playing a note to record it as a loop, and hold `KP-` to clear loops.

### Timing test
`cc/tests/run_timing_test.sh [seconds]` runs the real `MasterClock`, `AudioLooper` and `AudioPlayer` code headless. It records to a WAV instead of the speakers and adds a random 0-20 ms of CPU load on every tick. `check_timing.py` then checks that every click landed on its scheduled sample.

To build the proram, in the `/cc/` directory, the `build.sh` file will build all objects and link them into the `audio_player` object. It also
will generate an executable ready to be called for runtime. `build.sh` only checks each file's own `.cc`/`.h` for changes, not the headers it includes, so run `cleanbuild.sh` first after pulling changes. The permissions will need set on the `build.sh` file to set it as executable.

In the python directory, there are analytical scripts. There are two files:
`/python/looperDurationChecker.py` will provide data and plots on the cycle rates of different aspects of the program.

`/python/analzeLogFile.py` will provide overall metrics on the runtime of the program.

The `looperDurationChecker.py` takes 2 arguments:
```
-t, --type: options include: "kp": Keypad Progression, "pp": Program Progression (main cycle rate), "pd": Program time correciton durations
-f, --file: the filepath of the program log
```

When running the program `audio_player` it saves a log to the source directoy titled `duration_log` followed by a time stamp.

The "audio_player" program runs based entirely on configuration. All values are abstracted out of the code and initialized on start up.
# Part 1: 
```
audioMixer:
  mixer_sample_rate: 48000
  mixer_channels: 8               # number of GStreamer voices (simultaneous sounds)
  gst_sink: "autoaudiosink"       # optional; e.g. "alsasink device=hw:0 buffer-time=20000 latency-time=5000"
  gst_mixer_latency_ms: 10        # optional; how long audiomixer waits for late buffers
  scheduler_lookahead_ms: 30      # optional; how far ahead of each tick the scheduler runs
  # mixer_buffer_size and audio_format are ignored now (kept so old configs still load)
```

# Part 2:
```
window:
  font: "*.ttf" # system font ttf file
  fontSize: 24
  g1Width: 100
  g1Height: 200
  g2Width: 100
  g2Height: 400
  p2Width: 300
  p2Height: 400
  noteWidth: 200
```

# Part 3
```
bpm: 130.0
num_samples: 200
beatDivisions: 4.0 # this determines the cycle rate of the system scheduler. The formula is "60/bpm/beatDivisions"
kp1LoopDuration: 16.0 #4 bars
kp2LoopDuration: 8.0 #2 bars
kp3LoopDuration: 6.0 #1.5 bars
kp4LoopDuration: 4.0 #1 bars
kp5LoopDuration: 3.0
kp6LoopDuration: 2.5
kp7LoopDuration: 2.0
kp8LoopDuration: 1.5
kp9LoopDuration: 1.0
```

# Part 4
```
verbosity:
  timeVerbose: true
  superVerbose: true
  mainVerbose: false
  masterClockVerbose: false
  keyboardEventVerbose: false
  # Management
  managerVerbose: true
  # false
  looperVerbosity:
    looperManagerVerbose: false
    audioLooperVerbose: false
    graphicLooperVerbose: false
  # Process
  # Graphic
  graphicVerbosity:
    graphicManagerVerbose: false
    graphicProcessorVerbose: false
    graphicPlayerVerbose: false
  # Audio
  audioVerbosity:
    audioManagerVerbose: false
    audioProcessorVerbose: false
    audioPlayerVerbose: false
```

# Part 5
```
notes: 
  fn08A#4:
    filepath: "..." # must contain quotes. Filepath to sample audio file
    fnNumber: "fn08" # function key which needs active to play audio for a given key
    keycode: 13 # SDL keycode
```

To determine the SDL Keycode, there is a file in `/cc/` name `getKeyboardMapping.cc`. To build this file use:
```
g++ -o mapping getKeyboardMapping.cc -lSDL2
```

Set the permissions to `mapping` to exectuable and run the program. Make a keystroke and the program will print the SDL Keycode to terminal. There should also be a 
`keycodeReference.txt` within the Melydy package which can be used to reference the keycodes.
