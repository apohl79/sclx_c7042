#!/bin/bash
num_running=$(ps -ef|grep omxplayer|grep $1|wc -l)
if [ $num_running -gt 2 ]; then
    exit;
fi
export LD_LIBRARY_PATH=/opt/vc/lib:/usr/lib/omxplayer
/usr/bin/omxplayer.bin -o both $1 &
pid=$!
sleep 30
kill $pid >/dev/null 2>&1

