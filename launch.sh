#!/bin/sh
cd /mnt/SDCARD/App/CoGSDL/

if pgrep coGSDL > /dev/null; then
    killall -9 coGSDL
fi

/mnt/SDCARD/App/CoGSDL/coGSDL