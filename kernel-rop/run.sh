#!/bin/sh

initrd=${1:-"initramfs.cpio.gz"}

qemu-system-x86_64 \
    -m 128M \
    -cpu kvm64,+smep,+smap \
    -kernel vmlinuz \
    -initrd $initrd \
    -hdb flag.txt \
    -snapshot \
    -nographic \
    -monitor /dev/null \
    -no-reboot \
    -s \
    -append "console=ttyS0 nosmep nosmap nopti nokaslr quiet panic=1" # No kernel protections
    # -append "console=ttyS0 kaslr kpti=1 quiet panic=1" # kaslr enabled
