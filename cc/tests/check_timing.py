"""Check a timing_test recording: every click must land on its grid sample.

usage: python3 check_timing.py <out.wav> <schedule.csv> <duration_log.txt>
"""
import array
import struct
import sys

RATE = 48000
TOLERANCE_FRAMES = 1   # PTS -> sample rounding


def read_wav(path):
    data = open(path, 'rb').read()
    pos, fmt = 12, None
    while pos + 8 <= len(data):
        chunk_id = data[pos:pos + 4]
        size = struct.unpack('<I', data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if chunk_id == b'fmt ':
            fmt = struct.unpack('<HHIIHH', body[:16])
        elif chunk_id == b'data':
            tag, channels, rate, _, _, bits = fmt
            width = bits // 8
            samples = array.array('f' if tag == 3 else 'h', body[:len(body) // width * width])
            scale = 1.0 if tag == 3 else 32768.0
            return rate, channels, samples, scale
        pos += 8 + size + (size & 1)
    raise SystemExit('no data chunk in ' + path)


def find_onsets(samples, channels, scale, threshold=0.3, refractory=200):
    onsets, last_loud = [], -10**9
    for i in range(0, len(samples), channels):
        frame = i // channels
        if abs(samples[i]) / scale > threshold:
            if frame - last_loud > refractory:
                onsets.append(frame)
            last_loud = frame
    return onsets


def main():
    wav_path, csv_path, log_path = sys.argv[1:4]
    rate, channels, samples, scale = read_wav(wav_path)
    assert rate == RATE, rate
    total_frames = len(samples) // channels

    loops, end_ns, division_ns = [], None, None
    for line in open(csv_path).read().splitlines()[1:]:
        kind, a, b = line.split(',')
        if kind == 'loop':
            loops.append((int(a), int(b)))
        elif kind == 'end':
            end_ns = int(a)
        elif kind == 'division':
            division_ns = int(a)

    # Expected hits: the live press at T0, then T0 + n*interval (integer ns,
    # exactly like the scheduler's repeated addition). Stop 0.5 s before
    # the end so we don't count hits cut off by shutdown.
    last_ns = min(end_ns, total_frames * 10**9 // RATE) - 500_000_000
    expected = set()
    for start, interval in loops:
        t = start
        while t <= last_ns:
            expected.add(round(t * RATE / 1e9))
            t += interval

    onsets = [f for f in find_onsets(samples, channels, scale) if f * 10**9 / RATE <= last_ns + 1e6]
    onset_set = sorted(onsets)

    errors, missing = [], []
    for frame in sorted(expected):
        nearest = min(onset_set, key=lambda o: abs(o - frame)) if onset_set else None
        if nearest is None or abs(nearest - frame) > TOLERANCE_FRAMES:
            missing.append(frame)
        else:
            errors.append(nearest - frame)
    extra = [o for o in onset_set if min(abs(o - e) for e in expected) > TOLERANCE_FRAMES]

    # Every onset should also sit on the beat grid itself.
    grid_err = [abs(o - round(round(o * 1e9 / RATE / division_ns) * division_ns * RATE / 1e9)) for o in onset_set]

    leads = []
    for line in open(log_path):
        if line.startswith('MasterClock::executeScheduledBatches::Lead:'):
            leads.append(int(line.split()[1]))
    skipped = sum(1 for line in open(log_path) if 'Skipped:' in line)

    print(f'recording: {total_frames / RATE:.1f} s, {len(loops)} loopers')
    print(f'expected hits: {len(expected)}   detected onsets: {len(onset_set)}')
    print(f'matched: {len(errors)}   missing: {len(missing)}   extra: {len(extra)}')
    if errors:
        print(f'onset error vs scheduled sample: max {max(map(abs, errors))} frame(s)')
    print(f'worst distance from beat grid: {max(grid_err) if grid_err else "n/a"} frame(s)')
    if leads:
        print(f'scheduler lead (tick time - wake time): min {min(leads) / 1000:.1f} ms, '
              f'max {max(leads) / 1000:.1f} ms over {len(leads)} batch runs; ticks skipped: {skipped}')
    ok = not missing and not extra and len(errors) > 0
    print('PASS' if ok else 'FAIL')
    if missing:
        print('first missing frames:', missing[:10])
    if extra:
        print('first extra frames:', extra[:10])
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
