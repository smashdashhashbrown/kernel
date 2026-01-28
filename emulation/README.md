# Lighteight Qemu Kernel Emulation

## Description

This is ripped from the kernel-rop challenge, but here is are scripts to help with the emulation of customized kernels. Provided are the following:

- `initramfs` directory: Minimal filesystem with busybox can that can compressed and fed into the qemu-system script for emulation
- `compress.sh`: compresses `initramfs` directory into `initramfs.cpio.gz` needed for emulation `run.sh` script
- `decomp.sh`: decompresses initramfs.cpio.gz
- `extract-vmlinux.sh`: extracts `vmlinux` from compressed image
- `run.sh`: `qemu-system` bash script for emulating kernels. This will drop you into the emulated kernel and filesystem.

## Steps:

1) First you'll need to build a kernel compressed `bzImage` or `vmlinuz` that you want to emulate. This is a seperate topic but the `custom_building` directory in the root of this repository may be of aid.
2) Create a `initramfs.cpio.gz` file using the `compress.sh`. This file will be used as the initial ram disk for the emulated kernel.
    - Put whatever files you need into the `initramfs` directory before compression (i.e. kernel modules to test, debugging tools, etc.)
3) Execute `run.sh` and you will be dropped into an emulated busybox shell running under your custom kernel.

## Notes:

Kernel versions tested:
- 3.10.18
- 5.9.0-rc6

