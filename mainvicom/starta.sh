#!/bin/bash
arecord -c 1 -f S16_LE -r 8000 -t raw | ffmpeg -f s16le -acodec pcm_s16le -ac 1 -ar 8000 -probesize 32768 -analyzeduration 32768 -fflags nobuffer -flags low_delay -avioflags direct -i pipe: -c:a copy -probesize 32768 -analyzeduration 32768 -fflags nobuffer -flags low_delay -avioflags direct -f s16le udp://$1:8102?pkt_size=1024
