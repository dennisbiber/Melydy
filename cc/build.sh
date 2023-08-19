#!/bin/bash

# Function to compile individual source files
compile_source() {
    echo "Oi! Compilin' $1, in'it..."
    if ! g++ -O2 -g -c "$1" -I/usr/include/soundtouch -I. -o ${1%.cc}.o -lSDL2 -lsndfile -lfftw -pg -lstdc++fs -lasound -lm; then
        echo "Blimey! Compilin' $1 failed, it did!"
        exit 1
    fi
    echo "Done compilin' $1, mate!"
}

# Function to link object files and create the executable
link_objects() {
    echo "Gawd, linkin' them object files now..."
    if ! g++ -O3 -o audio_player \
        ScheduleAction.o \
        BatchActions.o \
        KeyboardEvent.o \
        MasterClock.o \
        AudioProcessor.o \
        AudioPlayer.o \
        AudioManager.o \
        GraphicProcessor.o \
        GraphicPlayer.o \
        GraphicManager.o \
        Manager.o \
        AudioLooper.o \
        LooperManager.o \
        main.o \
        -lstdc++fs \
        -L/usr/lib/arm-linux-gnueabihf \
        -lSDL2 \
        -lSDL2main \
        -lSDL2_mixer \
        -lpthread \
        -lyaml-cpp \
        -lsamplerate \
        -lSDL2_ttf \
        -lSoundTouch \
        -lsndfile \
        -lfftw3 \
        -lasound \
        -pg \
        -lm;
    then
        echo "Cor blimey! Linkin' failed, it did!"
        exit 1
    fi
    echo "Linked 'em up, mate! We got our audio player!"
}

# Function to compute MD5 checksum of a file
get_md5sum() {
    md5sum "$1" | awk '{print $1}'
}

# Check if the MD5 checksum of the object file matches the stored checksum
check_md5sum() {
    if [ -f "$1.md5" ]; then
        stored_md5=$(cat "$1.md5")
        current_md5=$(get_md5sum "$1")
        if [ "$stored_md5" == "$current_md5" ]; then
            echo "$1: Already compiled, skipping..."
            return 0
        fi
    fi
    return 1
}

# Compile source files and headers if necessary
if ! check_md5sum ScheduleAction.cc || ! check_md5sum ScheduleAction.h; then
    compile_source ScheduleAction.cc
    get_md5sum ScheduleAction.cc > ScheduleAction.cc.md5
    get_md5sum ScheduleAction.h > ScheduleAction.h.md5
fi

if ! check_md5sum BatchActions.cc || ! check_md5sum BatchActions.h; then
    compile_source BatchActions.cc
    get_md5sum BatchActions.cc > BatchActions.cc.md5
    get_md5sum BatchActions.h > BatchActions.h.md5
fi

if ! check_md5sum MasterClock.cc || ! check_md5sum MasterClock.h; then
    compile_source MasterClock.cc
    get_md5sum MasterClock.cc > MasterClock.cc.md5
    get_md5sum MasterClock.h > MasterClock.h.md5
fi

if ! check_md5sum AudioLooper.cc || ! check_md5sum AudioLooper.h; then
    compile_source AudioLooper.cc
    get_md5sum AudioLooper.cc > AudioLooper.cc.md5
    get_md5sum AudioLooper.h > AudioLooper.h.md5
fi

if ! check_md5sum LooperManager.cc || ! check_md5sum LooperManager.h; then
    compile_source LooperManager.cc
    get_md5sum LooperManager.cc > LooperManager.cc.md5
    get_md5sum LooperManager.h > LooperManager.h.md5
fi

if ! check_md5sum AudioProcessor.cc || ! check_md5sum AudioProcessor.h; then
    compile_source AudioProcessor.cc
    get_md5sum AudioProcessor.cc > AudioProcessor.cc.md5
    get_md5sum AudioProcessor.h > AudioProcessor.h.md5
fi

if ! check_md5sum AudioPlayer.cc || ! check_md5sum AudioPlayer.h; then
    compile_source AudioPlayer.cc
    get_md5sum AudioPlayer.cc > AudioPlayer.cc.md5
    get_md5sum AudioPlayer.h > AudioPlayer.h.md5
fi

if ! check_md5sum AudioManager.cc || ! check_md5sum AudioManager.h; then
    compile_source AudioManager.cc
    get_md5sum AudioManager.cc > AudioManager.cc.md5
    get_md5sum AudioManager.h > AudioManager.h.md5
fi

if ! check_md5sum GraphicProcessor.cc || ! check_md5sum GraphicProcessor.h; then
    compile_source GraphicProcessor.cc
    get_md5sum GraphicProcessor.cc > GraphicProcessor.cc.md5
    get_md5sum GraphicProcessor.h > GraphicProcessor.h.md5
fi

if ! check_md5sum GraphicPlayer.cc || ! check_md5sum GraphicPlayer.h; then
    compile_source GraphicPlayer.cc
    get_md5sum GraphicPlayer.cc > GraphicPlayer.cc.md5
    get_md5sum GraphicPlayer.h > GraphicPlayer.h.md5
fi

if ! check_md5sum GraphicManager.cc || ! check_md5sum GraphicManager.h; then
    compile_source GraphicManager.cc
    get_md5sum GraphicManager.cc > GraphicManager.cc.md5
    get_md5sum GraphicManager.h > GraphicManager.h.md5
fi

if ! check_md5sum KeyboardEvent.cc || ! check_md5sum KeyboardEvent.h; then
    compile_source KeyboardEvent.cc
    get_md5sum KeyboardEvent.cc > KeyboardEvent.cc.md5
    get_md5sum KeyboardEvent.h > KeyboardEvent.h.md5
fi

if ! check_md5sum Manager.cc || ! check_md5sum Manager.h; then
    compile_source Manager.cc
    get_md5sum Manager.cc > Manager.cc.md5
    get_md5sum Manager.h > Manager.h.md5
fi

if ! check_md5sum main.cc; then
    compile_source main.cc
    get_md5sum main.cc > main.cc.md5
fi

# Link object files to create the executable
link_objects

# Change the permissions of the executable
chmod 777 audio_player

echo "Good on ya, mate! The audio player's all set up! 'Ave a goo-day now!"
