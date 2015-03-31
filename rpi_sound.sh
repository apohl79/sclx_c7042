#!/bin/bash
export LD_LIBRARY_PATH=/opt/vc/lib:/usr/lib/omxplayer
/usr/bin/omxplayer.bin -o both $1 &
pid=$!
sleep 30
kill $pid >/dev/null 2>&1

