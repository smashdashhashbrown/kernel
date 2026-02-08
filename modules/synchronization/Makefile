obj-m += mutex.o
obj-m += spinlock.o
obj-m += rwlock.o
obj-m += atomic.o

PWD := $(CURDIR)

all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
