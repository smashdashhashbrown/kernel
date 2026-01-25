/*
 * chardev.c - Creates a read-only char device that says how many times
 *             you have read from the dev file
 */

#include <linux/atomic.h>
#include <linux/cdev.h>

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>

#include <linux/init.h>
#include <linux/kernel.h>   /* for sprintf() */

#include <linux/module.h>
#include <linux/printk.h>
#include <linux/types.h>

#include <linux/uaccess.h>  /* for get_usr and put_user */
#include <linux/version.h>

#include <asm/errno.h>

/* Prototypes - this should be in an h file */

static int device_open(struct inode *, struct file *);

static int device_release(struct inode *, struct file *);

static ssize_t device_read(struct file *, char __user *, size_t, loff_t *);

static ssize_t device_write(struct file *, const char __user *, size_t, loff_t *);


#define DEVICE_NAME "chardev"   /* Dev name as it appears in /proc/devices */

#define BUF_LEN 80              /* Max length of message from the device */


/* Global variables are declared as static, so are global within the file. */

static int major; // Major number assigned to our device driver

// Use the below method to enforce exclusive access to the device driver.
// Using atomic Compare-And-Swap (CAS) to maintain two states: 
//  CDEV_NOT_USED and CDEV_EXCLUSIVE_OPEN
// to determine whether the file is currently opened by someone or not.
//
// CAS compares the contents of a memory location with the expected value,
// and ,only if they are the same, modifies the contents of that memory location
// to the desired value.
enum {
    CDEV_NOT_USED,
    CDEV_EXECLUSIVE_OPEN,
};

// Is the device open? Used to prevent multiple accesses to device
static atomic_t already_open = ATOMIC_INIT(CDEV_NOT_USED);

static char msg[BUF_LEN + 1];

static struct class *cls;

static struct file_operations chardev_fops = {
    .read       = device_read,
    .write      = device_write,
    .open       = device_open,
    .release    = device_release,
};


static int __init chardev_init(void) {
    // Associates a character major number with set of driver entry points.
    // file_operations struct must contain pointers to function the driver
    // uses to implement the knerl interface to the driver.
    major = register_chrdev(0, DEVICE_NAME, &chardev_fops);

    if (major < 0) {
        pr_alert("Registering char device failed with %d\n", major);
        return major;
    }

    pr_info("I was assigned major number %d.\n", major);

    // Creates a class struct pointer for the device which will be used in
    // device create
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    cls = class_create(DEVICE_NAME);
#else
    cls = class_create(THIS_MODULE, DEVICE_NAME);
#endif

    /* 
        Function used to create a struct device in sysfd, registered
        to the specified class.
    */
    device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);

    pr_info("Device created on /dev/%s\n", DEVICE_NAME);
    return 0;
}


static void __exit chardev_exit(void) {
    device_destroy(cls, MKDEV(major, 0));
    class_destroy(cls);
    // Unregister the device
    unregister_chrdev(major, DEVICE_NAME);
}

// **********************************************
// METHODS
// **********************************************


// Called when a process tries to open the devicee file, e.g.
//  sudo cat /dev/chardev
static int device_open(struct inode *inode, struct file *file) {
    static int counter = 0;

    pr_info("[%s] device open.\n", DEVICE_NAME);

    // Reads the 32 bit value stored at location pointer: compares the value
    // to CDEV_NOT_USED and replaces value if equal.
    // Function returns old (value stored in ptr). 0 if CDEV_NOT_USED.
    if (atomic_cmpxchg(&already_open, CDEV_NOT_USED, CDEV_EXECLUSIVE_OPEN))
        return -EBUSY;

    sprintf(msg, "I already told you %d times Hello world!\n", counter++);
    return 0;
}


// Called when process closes device file
static int device_release(struct inode *inode, struct file *file) {
    // We're now ready for our next caller
    atomic_set(&already_open, CDEV_NOT_USED);
    return 0;
}


// Called when process attempts to read from file
static ssize_t device_read(struct file *filp,
                           char __user *buffer,
                           size_t length,
                           loff_t *offset) {
    // Number of bytes axtually written to the buffer
    int bytes_read = 0;
    const char *msg_ptr = msg;

    pr_info("[%s] device read.\n", DEVICE_NAME);

    if (!*(msg_ptr + *offset)) {
        // We are the end of message
        *offset = 0;    // reset the offset
        return 0;       // signify end of file
    }

    msg_ptr += *offset;

    // Actually put the data into the buffer
    while (length && *msg_ptr) {
        /*
        The buffer is in the user data segment, not the kernel.
        segment so "*" assignment won't work. We have to use `put_user`
        which copes data from the kernel data_segment to the user data
        segment
        */
       put_user(*(msg_ptr++), buffer++);

       length--;
       bytes_read++;
    }

    *offset += bytes_read;

    pr_info("[%s] device read exit.\n", DEVICE_NAME);
    // Most read functions return the number of bytes put into the buffer
    return bytes_read;
}


// Called when a process writes to dev file
//  echo "hi" | sudo tee /dev/chardev
static ssize_t device_write(struct file *filp,
                           const char __user *buff,
                           size_t len,
                           loff_t *off) {
    pr_alert("Sorry, this operation is not supported.\n");
    return -EINVAL;
}

module_init(chardev_init);
module_exit(chardev_exit);

MODULE_LICENSE("GPL");
