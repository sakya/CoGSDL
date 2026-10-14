#!/bin/sh
cd /mnt/SDCARD/App/CoGSDL/
export LD_LIBRARY_PATH=/mnt/SDCARD/App/CoGSDL/miyoo/lib:$LD_LIBRARY_PATH

# Free the audio device held by Onion's audioserver
. /mnt/SDCARD/.tmp_update/script/stop_audioserver.sh

if pgrep CoGSDL > /dev/null; then
    killall -9 CoGSDL
fi

./CoGSDL
