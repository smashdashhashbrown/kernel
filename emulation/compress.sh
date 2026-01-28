#!/bin/bash

set -ex

BDIR=${1:-"initramfs"}
OUT=${2:-"initramfs.cpio.gz"}
CURR=`pwd`

pushd $BDIR
find . -print0 | cpio --null --format=newc -o 2>/dev/null | gzip -9 > $OUT
mv $OUT $CURR
popd

