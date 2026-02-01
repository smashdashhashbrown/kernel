/*
 * sleep.c - create a /proc file, and if several processes try to open it
 *           at the same time, put all but one to sleep.
 */

#include <linux/atomic.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/version.h>

// For putting processes to sleep and waking them up
#include <linux/wait.h>

#include <asm/current.h>
#include <asm/errno.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
#define HAVE_PROC_OPS
#endif

// Using ENUM to represent is /proc file is in use or not
enum {
    CDEV_NOT_USED,
    CDEV_USED,
};


// Here we keep the last msg recieved, to prove that we can process our input
#define MESSAGE_LENGTH 80
static char message[MESSAGE_LENGTH + 1];

static struct proc_dir_entry *our_proc_file;

#define PROC_ENTRY_FILENAME "sleep"


/* Since we use the file operations struct, we can't use the special proc
 * output provisions - we have to use a standard read function, which is 
 * this function
 */
static ssize_t module_output(struct file *file,
                             char __user *buf,
                             size_t len,
                             loff_t *offset) {
    static int finished = 0;

    int i;
    char output_msg[MESSAGE_LENGTH + 30];

    /* Return 0 to signify end of file - that we have nothing more to say at this
     * point.
     */
    if (finished) {
        finished = 0;
        return 0;
    }

    snprintf(output_msg, MESSAGE_LENGTH + 30, "Last input:%s\n", message);

    for (i = 0; i < len && output_msg[i]; i++)
        put_user(output_msg[i], buf + i);

    finished = 1;
    return i; // Return the number of bytes read
}


/* This function recieves input from the user when the user writes to the
 * /proc file.
 */
static ssize_t module_input(struct file *file,
                            const char __user *buf,
                            size_t length,
                            loff_t *offset) {
    int i;

    /* Put the input into message, where module_output wil later be able
     * to use it.
     */
    for (i = 0; i < MESSAGE_LENGTH && i < length; i++) {
        get_user(message[i], buf + i);
    }
    message[i] = 0;

    // Return the number of input characters used
    return i;
}


// 1 if the file is currently open by somebody
static atomic_t already_open = ATOMIC_INIT(CDEV_NOT_USED);

// Queue of processes who want our file
static DECLARE_WAIT_QUEUE_HEAD(waitq);

// Called when the /proc file is opened
static int module_open(struct inode *inode, struct file *file) {
    // Try to get without blocking
    if (!atomic_cmpxchg(&already_open, CDEV_NOT_USED, CDEV_USED)) {
        // Success without blocking, allow entry
        return 0;
    }

    /* If the file's flags include O_NONBLOCK, it means the process does not
     * want to wait for the file. In this case, because the file is already open,
     * we should fail with -EAGAIN, meaning "you will have to try again",
     * instead of blocking a process which would rather stay awake.
     */
    if (file->f_flags & O_NONBLOCK)
        return -EAGAIN;

    while (atomic_cmpxchg(&already_open, CDEV_NOT_USED, CDEV_USED)) {
        int i, is_sig = 0;

        /* This function puts the current process, including any system
         * calls, such as us, to sleep. Execution will be resumed right after
         * the function call, either because somebody called
         * wake_up(&waitq) (only module_close does that, when the file is
         * closed) or when a signal, such as Ctrl-C, is sent to the process
         */
        wait_event_interruptible(waitq, !atomic_read(&already_open));

        /* If we woke up because we got a signal we're not blocking,
         * return -EINTR (fail the system call). This allows processes to be
         * killed or stopped.
         */
        for (i = 0; i < _NSIG_WORDS && !is_sig; i++)
            is_sig = current->pending.signal.sig[i] & ~current->blocked.sig[i];

        if (is_sig) {
            // Return -EINTR if we got a signal
            return -EINTR;
        }
    }
    return 0; // allow access
}


// Called when the /proc file is closed
static int module_close(struct inode *inode, struct file *file) {
    /* Set already_open to zero, so one of the processes in the waitq will
     * be able to set already_open back to one and to open the file. All the
     * other processes will be called when already_open is back to one, so
     * they'll go back to sleep.
     */
    atomic_set(&already_open, CDEV_NOT_USED);

    /* Wake up all the processes in waitq, so if anybody is waiting for the
     * file, they can have it.
     */
    wake_up(&waitq);

    return 0; // success
}


/* Structures to register as the /proc file, with pointers to all the relevant
 * functions.
 *
 * File operations for our /proc file. This is where we place pointers to all
 * the functions called when somebody tries to do something to our file. NULL
 * means we don't want to deal with something.
 */
#ifdef HAVE_PROC_OPS

static const struct proc_ops ops = {
    .proc_read      = module_output,
    .proc_write     = module_input,
    .proc_open      = module_open,
    .proc_release   = module_close,
    .proc_lseek     = noop_llseek,      // Return file->f_pos
};

#else  // !HAVE_PROC_OPS

static const struct file_operations ops = {
    .read       = module_output,
    .write      = module_input,
    .open       = module_open,
    .release    = module_close,
    .llseek     = noop_llseek,
};

#endif // HAVE_PROC_OPS


// Initialize the module - register the /proc file
static int __init sleep_init(void) {
    our_proc_file = proc_create(PROC_ENTRY_FILENAME, 0644, NULL, &ops);

    if (our_proc_file == NULL) {
        pr_debug("[%s] Error: Could not initalize /proc/%s\n", PROC_ENTRY_FILENAME, PROC_ENTRY_FILENAME);
        return -ENOMEM;
    }

    proc_set_size(our_proc_file, 80);
    proc_set_user(our_proc_file, GLOBAL_ROOT_UID, GLOBAL_ROOT_GID);

    pr_info("[%s] /proc/%s created\n", PROC_ENTRY_FILENAME, PROC_ENTRY_FILENAME);

    return 0;
}


/* Cleanup - unregister our file from /proc. This could get dangerous if there
 * are still processes waiting in waitq, because they are inside our open function,
 * which will get unloaded. I'll explain how to avoid removal of a kernel module
 * in such a case in chapter 10.
 */
static void __exit sleep_exit(void) {
    remove_proc_entry(PROC_ENTRY_FILENAME, NULL);
    pr_debug("/proc/%s removed\n", PROC_ENTRY_FILENAME);
}


module_init(sleep_init);
module_exit(sleep_exit);

MODULE_LICENSE("GPL");

