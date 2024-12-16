#!/bin/bash

clear

CFLAGS="-Os" LDFLAGS="-L$(pwd)/../../alsa-lib/build/install/usr/lib" CPPFLAGS="-I$(pwd)/../include -I$(pwd)/../../alsa-lib/build/install/usr/include -Os" CC=arm-linux-gnueabi-gcc ../configure --host=arm-linux-gnueabi --prefix="/usr" --with-softfloat --enable-shared=yes --enable-static=no --without-debug --without-versioned

make -j4

rm -r install

mkdir install

make DESTDIR="$(pwd)/install" install

arm-linux-gnueabi-strip -g -s "$(pwd)/install/usr/lib"/*.so
