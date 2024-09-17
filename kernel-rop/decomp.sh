#!/bin/bash

BDIR="initramfs"
FS="initramfs.cpio.gz"

mkdir $BDIR
cp $FS $BDIR/
pushd $BDIR

gzip -dc $FS | cpio -idm &>/dev/null
rm $FS
popd

