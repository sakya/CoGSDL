#!/bin/sh
cd /mnt/SDCARD/App/CoGSDL/

if pgrep CogSDL > /dev/null; then
    killall -9 CoGSDL
fi

/mnt/SDCARD/App/CoGSDL/CoGSDL
