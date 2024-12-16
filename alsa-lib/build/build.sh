#!/bin/bash

clear

echo "slibdir=/lib" >> configparms
echo "rtlddir=/lib" >> configparms
echo "sbindir=/bin" >> configparms
echo "rootsbindir=/bin" >> configparms

CFLAGS="-Os" CPPFLAGS="-Os" CC=arm-linux-gnueabi-gcc ../configure --host=arm-linux-gnueabi --target=arm-linux-gnueabi --libdir="/usr/arm-linux-gnueabi/lib" --prefix="/usr" --with-softfloat --enable-shared=yes --enable-static=no --without-debug --without-versioned

sed -i -e 's/ -shared / -Wl,-Os,--as-needed\0/g' libtool

make -j4

rm -r install

mkdir install

make DESTDIR="$(pwd)/install" install

arm-linux-gnueabi-strip -g -s "$(pwd)/install/usr/arm-linux-gnueabi/lib"/*.so
