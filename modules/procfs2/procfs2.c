/*
 * procfs2.c - Create a file in /rpco
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#define MOD_NAME "procfs2"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
#define HAVE_PROC_OPS
#endif

#define PROCFS_MAX_SIZE 1024
#define PROCFS_NAME "buffer1k"

// This structure holds info about the /proc file
static struct proc_dir_entry *our_proc_file;

// The buffer used to store character for this module
static char procfs_buffer[PROCFS_MAX_SIZE];

// The buffer used to store character for this module
static unsigned long procfs_buffer_size = 0;


// This function is called then the /proc file is read
static ssize_t procfile_read(struct file *file_pointer,
                             char __user *buffer,
                             size_t buffer_length,
                             loff_t *offset) {
    char s[] = "HelloWorld!\n";
    int len = sizeof(s);
    ssize_t ret = len;

    if (*offset >= len || copy_to_user(buffer, s, len)) {
        ret = 0;
    } else {
        pr_info("[%s] procfile read %s\n", MOD_NAME, file_pointer->f_path.dentry->d_name.name);
        *offset += len;
    }

    return ret;
}


// This function is called when the /proc file is written
static ssize_t procfile_write(struct file *file,
                              const char __user *buff,
                              size_t len,
                              loff_t *off) {
    procfs_buffer_size = len;
    
    if (procfs_buffer_size >= PROCFS_MAX_SIZE)
        procfs_buffer_size = PROCFS_MAX_SIZE - 1;

    if (copy_from_user(procfs_buffer, buff, procfs_buffer_size))
        return -EFAULT;

    procfs_buffer[procfs_buffer_size] = '\0';
    *off += procfs_buffer_size;

    pr_info("[%s] procfile write \"%s\" for %lu bytes", MOD_NAME, procfs_buffer, procfs_buffer_size); 
    return procfs_buffer_size;
}


#ifdef HAVE_PROC_OPS

static const struct proc_ops proc_file_fops = {
    .proc_read  = procfile_read,
    .proc_write = procfile_write,
};

#else

static const struct file_operations proc_file_fops = {
    .read  = procfile_read,
    .write = procfile_write,
}

#endif


static int __init procfs2_init(void) {
    // creates entries in the /proc fs
    our_proc_file = proc_create(PROCFS_NAME, 0644, NULL, &proc_file_fops);

    if (NULL == our_proc_file) {
        pr_alert("[%s] Error : Could not initialize /proc/%s\n", MOD_NAME, PROCFS_NAME);
        return -ENOMEM;
    }

    pr_info("/proc/%s created\n", PROCFS_NAME);

    return 0;
}


static void __exit procfs2_exit(void) {
    proc_remove(our_proc_file);
    pr_info("[%s] /proc/%s removed\n", MOD_NAME, PROCFS_NAME);
}

module_init(procfs2_init);
module_exit(procfs2_exit);

MODULE_LICENSE("GPL");

