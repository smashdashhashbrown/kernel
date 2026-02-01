obj-m += sleep.o

PWD := $(CURDIR)

CFLAGS = -Wall -g

all: userland
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

userland:
	$(CC) cat_nonblock.c -o cat_nonblock $(CFLAGS)

clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f cat_nonblock
