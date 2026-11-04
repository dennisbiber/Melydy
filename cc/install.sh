#!/bin/bash
# Melydy setup: check dependencies, download the piano samples, build.
#
#   ./install.sh          check, fetch samples into samples/piano/, build
#   ./install.sh --deps   same, but first apt-get install anything missing (sudo)
#
# Then run:  ./audio_player -c config1.yml
set -e
cd "$(dirname "$0")"

INSTALL_DEPS=0
[ "$1" == "--deps" ] && INSTALL_DEPS=1

missing=()
need_pkg() {  # pkg-config module, apt package
    pkg-config --exists "$1" 2>/dev/null || missing+=("$2")
}
need_cmd() {  # command, apt package
    command -v "$1" >/dev/null 2>&1 || missing+=("$2")
}

echo "== Checking dependencies"
need_cmd g++ g++
need_cmd pkg-config pkg-config
need_cmd python3 python3
need_pkg sdl2 libsdl2-dev
need_pkg SDL2_ttf libsdl2-ttf-dev
need_pkg yaml-cpp libyaml-cpp-dev
need_pkg gstreamer-1.0 libgstreamer1.0-dev
need_pkg gstreamer-app-1.0 libgstreamer-plugins-base1.0-dev
need_pkg gstreamer-audio-1.0 libgstreamer-plugins-base1.0-dev
[ -f /usr/share/fonts/truetype/freefont/FreeSans.ttf ] || missing+=(fonts-freefont-ttf)
if command -v gst-inspect-1.0 >/dev/null 2>&1; then
    for element in audiomixer appsrc decodebin audioconvert audioresample; do
        gst-inspect-1.0 "$element" >/dev/null 2>&1 || missing+=(gstreamer1.0-plugins-base)
    done
    gst-inspect-1.0 autoaudiosink >/dev/null 2>&1 || missing+=(gstreamer1.0-plugins-good)
    gst-inspect-1.0 mpg123audiodec >/dev/null 2>&1 || gst-inspect-1.0 avdec_mp3 >/dev/null 2>&1 \
        || missing+=(gstreamer1.0-plugins-good)   # mp3 decoding for the piano samples
    gst-inspect-1.0 pulsesink >/dev/null 2>&1 || missing+=(gstreamer1.0-pulseaudio)
else
    missing+=(gstreamer1.0-tools gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-pulseaudio)
fi
# de-duplicate
missing=($(printf "%s\n" "${missing[@]}" | sort -u))

if [ ${#missing[@]} -gt 0 ]; then
    if [ $INSTALL_DEPS -eq 1 ]; then
        echo "   Installing: ${missing[*]}"
        sudo apt-get install -y "${missing[@]}"
    else
        echo "   Missing packages: ${missing[*]}"
        echo "   Install them with:  sudo apt-get install ${missing[*]}"
        echo "   (or rerun:          ./install.sh --deps)"
        exit 1
    fi
else
    echo "   All dependencies present."
fi

echo "== Fetching piano samples for bank fn10"
python3 fetch_samples.py --config config1.yml --bank fn10 --dest samples/piano

echo "== Building"
chmod +x build.sh cleanbuild.sh
./build.sh

echo
echo "Done. Start Melydy with:  ./audio_player -c config1.yml"
echo "Piano is on bank fn10 (active at startup). Bluetooth output? See the gst_sink line in config1.yml."
