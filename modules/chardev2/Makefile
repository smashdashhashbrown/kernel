obj-m += chardev2.o

PWD := $(CURDIR)

CFLAG = -Wall -g

all: userland
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

userland:
	$(CC) userspace_ioctl.c -o userspace_ioctl $(CFLAGS)

clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f userspace_ioctl
