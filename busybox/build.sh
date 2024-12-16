#!/bin/bash

make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -j4

make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- CONFIG_PREFIX=../rootfs/files/ install
