#!/bin/bash
# Build and run the headless GStreamer timing test.
# usage: ./run_timing_test.sh [seconds]   (default 30)
set -e
cd "$(dirname "$0")"
SECONDS_TO_RUN=${1:-30}
WORK=$(mktemp -d)

GST_CFLAGS=$(pkg-config --cflags gstreamer-1.0 gstreamer-app-1.0 gstreamer-base-1.0 gstreamer-audio-1.0)
GST_LIBS=$(pkg-config --libs gstreamer-1.0 gstreamer-app-1.0 gstreamer-base-1.0 gstreamer-audio-1.0)
g++ -O2 $GST_CFLAGS -o "$WORK/timing_test" timing_test.cc \
    ../MasterClock.cc ../BatchActions.cc ../ScheduleAction.cc ../AudioLooper.cc \
    ../AudioPlayer.cc ../AudioProcessor.cc ../GstAudioEngine.cc ../StreamClock.cc \
    $GST_LIBS -lSDL2 -lyaml-cpp -lpthread

# A 5 ms click at 48 kHz mono: sharp first sample, quick decay. Mono on
# purpose, so the decode path's channel conversion gets exercised too.
python3 - "$WORK/click.wav" <<'EOF'
import math, struct, sys, wave
w = wave.open(sys.argv[1], 'wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(48000)
frames = [int(32000 * math.exp(-i / 40.0) * (1 if i % 8 < 4 else -1)) for i in range(240)]
w.writeframes(struct.pack('<%dh' % len(frames), *frames)); w.close()
EOF

cd "$WORK"
./timing_test click.wav out.wav schedule.csv "$SECONDS_TO_RUN"
python3 "$OLDPWD/check_timing.py" out.wav schedule.csv duration_logs_*.txt
echo "artifacts in $WORK"
