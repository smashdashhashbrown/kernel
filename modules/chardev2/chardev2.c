/*
 * chardev2.c - Create an input/output character device
 */

#include <linux/atomic.h>
#include <linux/cdev.h>  
#include <linux/delay.h>
#include <linux/device.h>  
#include <linux/fs.h>  
#include <linux/init.h>
#include <linux/module.h> /* Specifically, a module */  
#include <linux/printk.h>
#include <linux/types.h>  
#include <linux/uaccess.h> /* for get_user and put_user */
#include <linux/version.h>   
#include <asm/errno.h>    

#include "chardev.h"

#define DEVICE_NAME "char_dev"
#define BUF_LEN 80

enum {
    CDEV_NOT_USED,
    CDEV_EXCLUSIVE_OPEN,
};

/* 
 * Is the device open right now? Used to prevent concurrent access into
 * the same device.
 */
static atomic_t already_open = ATOMIC_INIT(CDEV_NOT_USED);

// The message the device will give when asked
static char message[BUF_LEN + 1];

static struct class *cls;


// This is called whenever a process attempts to open the device file
static int device_open(struct inode *inode, struct file *file) {
    pr_info("[%s] device_open(%p)\n", DEVICE_NAME, file);
    return 0;
}


static int device_release(struct inode *inode, struct file *file) {
    pr_info("[%s] device_release(%p,%p)\n", DEVICE_NAME, inode, file);
    return 0;
}


// Call when process which has already opened device file attempts to read
static ssize_t device_read(struct file *file,   // see include/linux/fs.h
                           char __user *buffer, // buffer to be filled
                           size_t length,
                           loff_t *offset) {
    // Number of bytes actually written to the buffer
    int bytes_read = 0;

    /*
     * How far did the process reading the message get? useful if the message
     * is larger than the size of the buffer we get to fill in device_read
     */
    const char *message_ptr = message;

    // Still don't understand as this works? Like offset was a huge value (
    // buffer than size of `message` in chardev1. I thought it would be an
    // offset to the message pointer but it's an offset to the file.
    // Offset is not 0 on initial/first read.
    if (!*(message_ptr + *offset) || (*offset + length > BUF_LEN)) {
        *offset = 0;
        return 0;
    }

    message_ptr += *offset;

    // Actually put the data into the buffer
    while (length && *message_ptr) {
        put_user(*(message_ptr++), buffer++);
        length--;
        bytes_read++;
    }

    pr_info("[%s] Read %d bytes, %ld left\n", DEVICE_NAME, bytes_read, length);
    *offset += bytes_read;
    return bytes_read;
}

// Called when somebody tries to write into our device file
static ssize_t device_write(struct file *file,
                            const char __user *buffer,
                            size_t length,
                            loff_t *offset) {
    int i;

    pr_info("[%s] device_write(%p,%p,%ld)\n", DEVICE_NAME, file, buffer, length);

    for (i = 0; i < length && i < BUF_LEN; i++)
        get_user(message[i], buffer + i);
    message[i] = '\0';

    // Return the number of input characters used
    return i;
}


/*
 * This function is called whenever a pro cess tries to do an ioctl on our
 * device file. We get two extra parameters (additional to the inode and file
 * structures, which all device functions get):
 *  - the number of the ioctl called
 *  - the parameter given to the ioctl
 * If the ioctl is write or read/write (meaning output is returned to the
 * calling process), the ioctl call returns the output of this function.
 */
static long device_ioctl(struct file *file,
                         unsigned int ioctl_num,
                         unsigned long ioctl_param) {
    int i;
    long ret;

    // We don't want to talk to two processes concurrently
    if (atomic_cmpxchg(&already_open, CDEV_NOT_USED, CDEV_EXCLUSIVE_OPEN))
        return -EBUSY;

    // Switch according to ioctl called
    switch (ioctl_num) {
        case IOCTL_SET_MSG:
            pr_info("[%s] IOCTL_SET_MSG\n", DEVICE_NAME);
            /* Recieve a pointer to a message (in user space) and set that
             * to be the device's message. Get the parameter given to ioctl by
             * the process.
             */
            char __user *tmp = (char __user *)ioctl_param;
            char ch;

            // Find the length of the message
            get_user(ch, tmp);
            // Iterate as long as ch isn't null and i < length of message buffer
            for (i = 0; ch && i < BUF_LEN; i++, tmp++)
                get_user(ch, tmp);

            device_write(file, (char __user *)ioctl_param, i, NULL);
            break;

        case IOCTL_GET_MSG:
            pr_info("[%s] IOCTL_GET_MSG\n", DEVICE_NAME);
            loff_t offset = 0;

            /* Given the current message to the calling process - the parameter
             * we got is a pointer, fill it.
             */
            i = device_read(file, (char __user *)ioctl_param, BUF_LEN, &offset);

            // Null terminate buffer
            put_user(0, (char __user *)ioctl_param + i);
            break;

        case IOCTL_GET_NTH_BYTE:
            pr_info("[%s] IOCTL_GET_NTH_BYTE\n", DEVICE_NAME);
            /* This ioctl is both input (ioctl_param) and output (the
             * return value of this function).
             */
            if (ioctl_param >= BUF_LEN)
                pr_alert("[%s] index %ld given for IOCTL_GET_NTH_BYTE out of bounds\n", DEVICE_NAME, ioctl_param);
            else
                ret = (long)message[ioctl_param];
            break;
    }

    atomic_set(&already_open, CDEV_NOT_USED);
    return ret;
}


/* Module Declarations
 *
 * This structure will hold the functions to be called when a process does
 * something to the device we created. Since a pointer to this structure
 * is kept in the devices table, it can't be local to init_module. NULL is
 * for unimplemented functions.
 */
static struct file_operations fops = {
    .read           = device_read,
    .write          = device_write,
    .unlocked_ioctl = device_ioctl,
    .open           = device_open,
    .release        = device_release,
};


// Initialize the module - register the character device
static int __init chardev2_init(void) {
    // Register the character device
    int ret_val = register_chrdev(MAJOR_NUM, DEVICE_NAME, &fops);

    if (ret_val < 0) {
        pr_alert("%s failed with %d", "register_chrdev", ret_val);
        return ret_val;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,4,0)
    cls = class_create(DEVICE_FILE_NAME);
#else
    cls = class_create(THIS_MODULE, DEVICE_FILE_NAME);
#endif

    device_create(cls, NULL, MKDEV(MAJOR_NUM, 0), NULL, DEVICE_FILE_NAME);

    pr_info("Device created on /dev/%s\n", DEVICE_FILE_NAME);
    return 0;
}


// Cleanup - unregister the appropriate file from /proc
static void __exit chardev2_exit(void) {
    device_destroy(cls, MKDEV(MAJOR_NUM, 0));
    class_destroy(cls);

    unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
}

module_init(chardev2_init);
module_exit(chardev2_exit);

MODULE_LICENSE("GPL");

