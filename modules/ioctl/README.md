# Device Files

Devices files are supposed to represent physical devices. Most physical devices are used for output as well as input. So there has to be some mechanism for device drivers in the kernel to get output to send to the device from the processes. This is done by opening the device file for output and writing to it.

Every device can have its own `ioctl` commands, which can be read ioctl's (to send information from a process to the kernel, write ioctl's (to return information to a process), both or neither. Notice here the roles of read and write are reversed (from the kernel's perspective), so in ioctl's read is to send info to the kernel and write is to recieve info from the kernel. 

The `ioctl` function is called with three parameters: the file descriptor of the appropriate device file, the ioctl number, and a parameter, which is of type long you can use a cast to use it to pass anything.

# IOCTL

When one creates their own driver that performs IOCTLs, he/she will need to descrive all this in the ioctl command.

`_IO`, `_IOW`, `_IOR`, `_IORW` are helper macros to create a unique ioctl identifier and add the required R/W needed features. These can take the folowing params: magic number, the command id, and the data type that will be passed (if any).
- The magic number is a unique number that will allow the driver to detect errors such as the one mentioned in the LDD book's quote below.
- The command id, is a way for your driver to understand what command is needed to be called.
- Last paramater will allow the kernel to understand the size to be copied

Linux Device Driver quote:
"""
The ioctl command numbers should be unique across the system in order to prevent errors caused by issuing the right command to the wrong device.Such a mismatch is not unlikely to happen, and a program might find itself trying to change the baudrate of a non-serial-port input stream, such as a FIFO or an audio device. If each ioctl number is unique, the application gets an EINVAL error rather than succeeding in doing something unintended.
"""
