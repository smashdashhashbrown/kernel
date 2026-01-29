obj-m += syscall-table-overwrite.o 

PWD := $(CURDIR)

all:
	$(MAKE) -C /$(shell pwd)/../kernel M=$(PWD) modules

clean:
	$(MAKE) -C /$(shell pwd)/../kernel M=$(PWD) clean
