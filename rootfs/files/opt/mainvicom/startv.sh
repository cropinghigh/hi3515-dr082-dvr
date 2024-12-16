#!/bin/bash
ffmpeg -probesize 32768 -analyzeduration 32768 -fflags nobuffer -flags low_delay -f mpegts -i udp://:8101?listen=1 -fflags nobuffer -flags low_delay -pix_fmt rgb555le -f fbdev /dev/fb0 &

ffmpeg -f s16le -acodec pcm_s16le -ac 1 -ar 8000 -probesize 32768 -analyzeduration 32768 -fflags nobuffer -flags low_delay -i udp://:8104?listen=1 -c:a copy -f s16le - | aplay --period-size=2048 --buffer-size=8192 -f S16_LE -r 8000 - &

wait
