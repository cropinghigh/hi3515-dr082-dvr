#!/bin/bash

mkfs.jffs2 --eraseblock=0x20000 --little-endian --no-cleanmarkers -r files -o rootfs
