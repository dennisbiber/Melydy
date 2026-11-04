#!/usr/bin/env python3
"""Download the piano samples a config bank needs, and point the config at them.

Reads the note names of one function bank in the config (e.g. fn10C#5 ->
C#5), downloads just those notes from the fuhton/piano-mp3 repo into a local
folder, and rewrites that bank's `filepath:` lines to the downloaded files.
Other banks are left as they are. Safe to re-run: files already downloaded
are skipped.

Paths are written relative to the config file's folder; Melydy resolves them
from there, so the config and samples can be moved together.

usage: python3 fetch_samples.py [--config config1.yml] [--bank fn10]
                                [--dest samples/piano] [--blank-other-banks]
"""
import argparse
import os
import re
import sys
import urllib.request

# Pinned so an upstream change can't break an existing setup.
REPO_COMMIT = "1238aa3635393ff143f9cc14643fd2d2ad66b8c4"
BASE_URL = f"https://raw.githubusercontent.com/fuhton/piano-mp3/{REPO_COMMIT}/piano-mp3/"

# The config names notes with sharps (A#4); the repo's files use flats (Bb4).
SHARP_TO_FLAT = {"C#": "Db", "D#": "Eb", "F#": "Gb", "G#": "Ab", "A#": "Bb"}

ENTRY_RE = re.compile(r'^  ([A-Za-z0-9#]+):\s*$')
FILEPATH_RE = re.compile(r'^(    filepath:\s*)(".*"|\S*)\s*$')
FNNUMBER_RE = re.compile(r'^    fnNumber:\s*"?([A-Za-z0-9]+)"?\s*$')
NOTE_RE = re.compile(r'^([A-G])(#?)(\d)(D?)$')  # trailing D = duplicate key for the same note


def note_file(entry_name, bank):
    """fn10C#5D -> 'Db5.mp3' (or None if the name isn't a note)."""
    m = NOTE_RE.match(entry_name[len(bank):])
    if not m:
        return None
    letter, sharp, octave, _ = m.groups()
    name = SHARP_TO_FLAT[letter + "#"] if sharp else letter
    return f"{name}{octave}.mp3"


def parse_entries(lines):
    """Yield (entry_name, filepath_line_index, fnNumber) for each note entry."""
    in_notes, current, fp_index, fn = False, None, None, None
    for i, line in enumerate(lines + ["<end>"]):
        if line.startswith("notes:"):
            in_notes = True
            continue
        if not in_notes:
            continue
        m = ENTRY_RE.match(line)
        if m or line == "<end>" or (line and not line.startswith(" ")):
            if current is not None:
                yield current, fp_index, fn
            current, fp_index, fn = (m.group(1) if m else None), None, None
            if not m and line != "<end>" and line and not line.startswith(" "):
                in_notes = False
            continue
        if current is not None:
            if FILEPATH_RE.match(line):
                fp_index = i
            fm = FNNUMBER_RE.match(line)
            if fm:
                fn = fm.group(1)


def download(name, dest_dir):
    target = os.path.join(dest_dir, name)
    if os.path.isfile(target) and os.path.getsize(target) > 0:
        return "cached"
    tmp = target + ".part"
    with urllib.request.urlopen(BASE_URL + name, timeout=30) as response, open(tmp, "wb") as out:
        out.write(response.read())
    os.replace(tmp, target)
    return "downloaded"


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--config", default="config1.yml")
    ap.add_argument("--bank", default="fn10")
    ap.add_argument("--dest", default="samples/piano", help="relative to the config's folder")
    ap.add_argument("--blank-other-banks", action="store_true",
                    help="set every other bank's filepath to \"\" (unassigned)")
    args = ap.parse_args()

    config_dir = os.path.dirname(os.path.abspath(args.config))
    dest_dir = os.path.join(config_dir, args.dest)
    os.makedirs(dest_dir, exist_ok=True)
    lines = open(args.config).read().split("\n")

    needed, rewrites, blanks, skipped = {}, {}, [], []
    for name, fp_index, fn in parse_entries(lines):
        if fp_index is None:
            continue
        if fn == args.bank:
            mp3 = note_file(name, args.bank)
            if mp3 is None:
                skipped.append(name)
                continue
            needed.setdefault(mp3, []).append(name)
            rewrites[fp_index] = os.path.join(args.dest, mp3)
        elif args.blank_other_banks:
            blanks.append(fp_index)

    if not needed:
        sys.exit(f"No note entries found for bank {args.bank} in {args.config}")

    print(f"Bank {args.bank}: {sum(len(v) for v in needed.values())} keys, {len(needed)} distinct notes")
    failed = []
    for mp3 in sorted(needed):
        try:
            status = download(mp3, dest_dir)
        except Exception as e:  # network error, 404, ...
            status = f"FAILED ({e})"
            failed.append(mp3)
        print(f"  {mp3:<8} {status}")

    for i, path in rewrites.items():
        lines[i] = FILEPATH_RE.sub(lambda m: f'{m.group(1)}"{path}"', lines[i])
    for i in blanks:
        lines[i] = FILEPATH_RE.sub(lambda m: f'{m.group(1)}""', lines[i])
    open(args.config, "w").write("\n".join(lines))

    print(f"Updated {args.config}: {len(rewrites)} paths in {args.bank}"
          + (f", {len(blanks)} other entries blanked" if blanks else ""))
    if skipped:
        print("  (not note names, left alone: " + ", ".join(skipped) + ")")
    if failed:
        sys.exit(f"{len(failed)} download(s) failed: " + ", ".join(failed))


if __name__ == "__main__":
    main()
