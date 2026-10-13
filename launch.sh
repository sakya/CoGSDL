#!/bin/sh
cd /mnt/SDCARD/App/CoGSDL/
export LD_LIBRARY_PATH=/mnt/SDCARD/App/CoGSDL/lib:$LD_LIBRARY_PATH
if pgrep CoGSDL > /dev/null; then
    killall -9 CoGSDL
fi

./CoGSDL
