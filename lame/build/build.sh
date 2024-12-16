#!/bin/bash

clear

../configure --host=arm-linux-gnueabi --prefix="/usr" --enable-static --disable-asm

make -j4
# 
rm -r install
# 
mkdir install
# 
make DESTDIR="$(pwd)/install" install
# 
arm-linux-gnueabi-strip -g -s "$(pwd)/install/usr/lib"/*.so
