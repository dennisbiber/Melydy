# Melydy
Drum Pad/Live Looper/Sequencer/Recording system/Synthesizer
Melyda is an open-source project with the end goal of providing a free solution for drum pad/Live Looper/AudioVisual Synthesis. The goal is also to have
this at production quality to integrate into any stage setup for live performance. 

Packages Need For Program:
```
libsdl2-dev
libsdl2-mixer-dev
libsdl2-ttf-dev
libyaml-cpp-dev
libasound2-dev
```

To build the proram, in the `/cc/` directory, the `build.sh` file will build all objects and link them into the `audio_player` object. It also
will generate an executable ready to be called for runtime. The permissions will need set on the `build.sh` file to set it as executable.

In the python directory, there are analytical scripts. There are two files:
`/python/looperDurationChecker.py` will provide data and plots on the cycle rates of different aspects of the program.

`/python/analzeLogFile.py` will provide overall metrics on the runtime of the program.

The `looperDurationChecker.py` takes 2 arguments:
```
-t, --type: options include: ["kp": Keypad Progression, "pp": Program Progression (main cycle rate), "pd": Program time correciton durations
-f, --file: the filepath of the program log
```

When running the program `audio_player` it saves a log to the source directoy titled `duration_log` followed by a time stamp.

The "audio_player" program runs based entirely on configuration. All values are abstracted out of the code and initialized on start up.
# Part 1: 
```
audioMixer:
  mixer_buffer_size: 2048
  mixer_sample_rate: 48000
  mixer_channels: 8
  audio_format: 2 # 1=mono, 2=stereo
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
verbose: false # sets all verbose true except super
timeVerbose: true
superVerbose: true
mainVerbose: false
masterClockVerbose: false
keyboardEventVerbose: false
# Management
managerVerbose: true
# false
looperManagerVerbose: false
audioLooperVerbose: false
graphicLooperVerbose: false
# Process
# Graphic
graphicManagerVerbose: false
graphicProcessorVerbose: false
graphicPlayerVerbose: false
# Audio
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
