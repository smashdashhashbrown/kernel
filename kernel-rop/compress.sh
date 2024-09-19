#!/bin/bash

set -ex

EXPLOIT=${1:-"exploit-kpti-tramp.c"}
BDIR=${2:-"initramfs"}
OUT=${3:-"initramfs-mod.cpio.gz"}
CURR=`pwd`

gcc $EXPLOIT -Wall -static -o $BDIR/exploit

pushd $BDIR
find . -print0 | cpio --null --format=newc -o 2>/dev/null | gzip -9 > $OUT
mv $OUT $CURR
popd

