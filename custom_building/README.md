# Building

Building old linux kernels on modern debian/ubuntu versions will give way to a relentless amount of errors.

My solution is just to pull an old version of ubuntu that ran these older linux kernels (ubuntu:12.04 ran v3's) and build them in those containers.

This has been used on:
- Ubuntu 24.04.1 LTS

This has been used to build kernel versions:
- v3.10.18

## Instructions

1) Build the docker from the Dockerfile if you haven't yet done so:

```
docker build -t <name>:<tag> .
```

2) Run the script

```
Usage: ./docker_build.sh -D build_dir -I docker_image -W working_dir <command to run>
	-D directory to mount into /build
	-I Docker image and tag to use and build within (default: kerbuntu:12.04)
	-W working directory within docker (default: /build)

EXAMPLE TO BUILD KERNEL

$ ./docker_build.sh -D ./v3.10.18/linux-3.10.18 make

EXAMPLE TO BUILD KERNEL MODULE

$ ././docker_build.sh -D ./v3.10.18/ -W /build/hello-1/ make

# ll ./v3.10.18/
total 16
drwxrwxr-x  4 luigi luigi 4096 Jan 28 14:33 ./
drwxrwxr-x  3 luigi luigi 4096 Jan 28 13:34 ../
drwxrwxr-x  3 luigi luigi 4096 Jan 28 14:34 hello-1/ # module build directory and working directory
drwxrwxr-x 24 luigi luigi 4096 Jan 28 13:46 kernel/  # Kernel build directory

# NOTE: It is important to configure the Makefile script in the module directory to point to the mount kernel build directory inside the docker in order to make this work

```