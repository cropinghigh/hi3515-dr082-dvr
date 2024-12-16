#!/bin/bash

clear

pkg_config=$(which pkg-config) PKG_CONFIG_PATH="$(pwd)/../../x264/build/install/usr/lib/pkgconfig" ../configure \
    --enable-cross-compile \
    --cross-prefix=arm-linux-gnueabi- \
    --arch=arm \
    --cpu=armv5te \
    --enable-armv5te \
    --disable-debug \
    --target-os=linux \
    --enable-gpl \
    --enable-shared \
    --disable-runtime-cpudetect \
    --disable-doc \
    --disable-everything \
    --enable-encoder=pcm_s16le \
    --enable-encoder=rawvideo \
    --enable-decoder=mp3 \
    --enable-decoder=pcm_s16le \
    --enable-decoder=mpeg1video \
    --enable-decoder=mpeg2video \
    --enable-muxer=mp4 \
    --enable-muxer=mp3 \
    --enable-muxer=pcm_s16le \
    --enable-muxer=mpegts \
    --enable-muxer=rawvideo \
    --enable-muxer=mpeg2video \
    --enable-muxer=mov \
    --enable-muxer=rtsp \
    --enable-demuxer=sdp \
    --enable-demuxer=mpegvideo \
    --enable-demuxer=rtsp \
    --enable-demuxer=mp3 \
    --enable-demuxer=mpegts \
    --enable-demuxer=pcm_s16le \
    --enable-demuxer=rawvideo \
    --enable-demuxer=mov \
    --enable-demuxer=sdp \
    --enable-parser=h264 \
    --enable-protocol=http \
    --enable-protocol=rtmp \
    --enable-protocol=rtp \
    --enable-protocol=tcp \
    --enable-protocol=udp \
    --enable-protocol=file \
    --enable-protocol=pipe \
    --enable-indev=fbdev \
    --enable-outdev=fbdev \
    --enable-filter=scale \
    --enable-filter=null \
    --enable-filter=aresample \
    --enable-filter=color \
    --enable-filter=copy \
    --enable-filter=fps \
    --enable-filter=format \
    --enable-filter=framerate \
    --enable-filter=nullsrc \
    --enable-filter=volume \
    --enable-filter=setpts \
    --disable-bsfs \
    --disable-iconv \
    --disable-sdl2  \
    --extra-cflags="-I$(pwd)/../../x264/build/install/usr/include/ -I$(pwd)/../../alsa-lib/build/install/usr/include/ -I$(pwd)/../../lame/build/install/usr/include/" \
    --extra-ldflags="-ldl -L$(pwd)/../../x264/build/install/usr/lib/ -L$(pwd)/../../alsa-lib/build/install/usr/arm-linux-gnueabi/lib/ -L$(pwd)/../../lame/build/install/usr/lib/" \
    --prefix="/usr"

make -j4

rm -r install

mkdir install

make DESTDIR="$(pwd)/install" install

arm-linux-gnueabi-strip -g -s "$(pwd)/install/usr/lib"/*.so
