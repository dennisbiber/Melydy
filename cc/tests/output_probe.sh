#!/bin/bash
# Which engine settings actually get sound to your speakers?
#
# Plays scripted key presses and loops through the real Melydy engine under
# several configurations, while recording your default output's *monitor*
# (exactly what PipeWire/PulseAudio sends to the device). Counts the clicks
# that made it through, so you don't have to listen. Takes about a minute.
#
# usage: ./output_probe.sh        (from cc/tests; needs parec: pulseaudio-utils)
set -e
cd "$(dirname "$0")"
command -v parec >/dev/null || { echo "parec not found: sudo apt-get install pulseaudio-utils"; exit 1; }
WORK=$(mktemp -d)
PKGS="gstreamer-1.0 gstreamer-app-1.0 gstreamer-base-1.0 gstreamer-audio-1.0"
g++ -O2 -w $(pkg-config --cflags $PKGS) -o "$WORK/timing_test" timing_test.cc \
    ../MasterClock.cc ../BatchActions.cc ../ScheduleAction.cc ../AudioLooper.cc \
    ../AudioPlayer.cc ../AudioProcessor.cc ../GstAudioEngine.cc ../StreamClock.cc \
    $(pkg-config --libs $PKGS) -lSDL2 -lyaml-cpp -lpthread
python3 - "$WORK/click.wav" <<'EOF'
import math, struct, sys, wave
w = wave.open(sys.argv[1], 'wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(48000)
frames = [int(32000 * math.exp(-i / 400.0) * math.sin(2 * math.pi * 1000 * i / 48000)) for i in range(4800)]
w.writeframes(struct.pack('<%dh' % len(frames), *frames)); w.close()
EOF

echo "Default output: $(pactl get-default-sink 2>/dev/null || echo '?')"
echo "Keep your volume at a normal level; each test plays ~10 s of clicks."
echo

run() {  # name, then env assignments
    local name=$1; shift
    rm -f "$WORK"/out.wav "$WORK"/schedule.csv "$WORK"/mon.wav "$WORK"/duration_logs_*
    parec -d @DEFAULT_MONITOR@ --file-format=wav --format=float32le --rate=48000 --channels=2 "$WORK/mon.wav" &
    local rec=$!
    sleep 0.5
    (cd "$WORK" && env MELYDY_NO_LOAD=1 "$@" ./timing_test click.wav out.wav schedule.csv 10 > run.log 2>&1) || true
    sleep 1; kill $rec 2>/dev/null; wait $rec 2>/dev/null || true
    python3 - "$name" "$WORK" <<'EOF'
import array, struct, sys
name, work = sys.argv[1], sys.argv[2]
def read(path):
    d = open(path, 'rb').read(); pos = 12; fmt = None
    while pos + 8 <= len(d):
        cid, size = d[pos:pos+4], struct.unpack('<I', d[pos+4:pos+8])[0]
        if cid == b'fmt ': fmt = struct.unpack('<HHIIHH', d[pos+8:pos+24])
        if cid == b'data':
            body = d[pos+8:]; tag, ch, rate, _, _, bits = fmt
            a = array.array('f' if bits == 32 else 'h', body[:len(body)//(bits//8)*(bits//8)])
            return rate, ch, a, (1.0 if bits == 32 else 32768.0)
        pos += 8 + size + (size & 1)
    return 48000, 2, array.array('f'), 1.0
expected = 0; end = None; loops = []; last_t = 0
for line in open(work + '/schedule.csv').read().splitlines()[1:]:
    k, a, b = line.split(',')
    if k == 'loop': loops.append((int(a), int(b)))
    if k == 'end': end = int(a)
times = []
for start, interval in loops:
    t = start
    while t <= end - 300_000_000: times.append(t); t += interval
# hits closer than 50 ms sound as one click: count them once
for t in sorted(times):
    if not expected or t - last_t > 50_000_000: expected += 1
    last_t = t
rate, ch, a, scale = read(work + '/mon.wav')
onsets, last = 0, -10**9
for i in range(0, len(a), ch):
    v = abs(a[i]) / scale
    if v > 0.1:
        if i // ch - last > 2400: onsets += 1
        last = i // ch
clock = ''.join(l.split('Clock: ')[1].split(',')[0] for l in open(work + '/run.log') if 'Clock:' in l)
lat = ''.join(l.split('pipeline latency: ')[1].strip() for l in open(work + '/run.log') if 'pipeline latency' in l)
verdict = 'OK' if expected and onsets >= expected * 0.9 else ('PARTIAL' if onsets else 'SILENT')
print(f'{name:<34} {onsets:>3}/{expected:<3} clicks reached the output   {verdict:<8} [{clock}, latency {lat}]')
EOF
}

run "1 current (device clock, feed 10ms)"   MELYDY_SINK=autoaudiosink MELYDY_SINK_CLOCK=1 MELYDY_FEED_AHEAD_MS=10
run "2 system clock, feed 10ms"             MELYDY_SINK=autoaudiosink MELYDY_SINK_CLOCK=0 MELYDY_FEED_AHEAD_MS=10
run "3 device clock, feed 300ms"            MELYDY_SINK=autoaudiosink MELYDY_SINK_CLOCK=1 MELYDY_FEED_AHEAD_MS=300
run "4 system clock, feed 300ms"            MELYDY_SINK=autoaudiosink MELYDY_SINK_CLOCK=0 MELYDY_FEED_AHEAD_MS=300
run "5 pulsesink 400ms buffer"              MELYDY_SINK="pulsesink buffer-time=400000 latency-time=20000" MELYDY_SINK_CLOCK=1 MELYDY_FEED_AHEAD_MS=10
if gst-inspect-1.0 pipewiresink >/dev/null 2>&1; then
    run "6 pipewiresink (native PipeWire)"  MELYDY_SINK=pipewiresink MELYDY_SINK_CLOCK=1 MELYDY_FEED_AHEAD_MS=10
else
    echo "6 pipewiresink                     skipped (sudo apt-get install gstreamer1.0-pipewire to include it)"
fi
echo
echo "Paste these lines back. (Work files: $WORK)"
