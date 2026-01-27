# Device Files

Devices files are supposed to represent physical devices. Most physical devices are used for output as well as input. So there has to be some mechanism for device drivers in the kernel to get output to send to the device from the processes. This is done by opening the device file for output and writing to it.

Every device can have its own `ioctl` commands, which can be read ioctl's (to send information from a process to the kernel, write ioctl's (to return information to a process), both or neither. Notice here the roles of read and write are reversed (from the kernel's perspective), so in ioctl's read is to send info to the kernel and write is to recieve info from the kernel. 

The `ioctl` function is called with three parameters: the file descriptor of the appropriate device file, the ioctl number, and a parameter, which is of type long you can use a cast to use it to pass anything.
