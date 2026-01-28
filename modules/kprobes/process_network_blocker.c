#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/file.h>


// Process you want to block
char *executable_path = "/usr/bin/wget";
module_param(executable_path, charp, 0000);
MODULE_PARM_DESC(executable_path, "Path to executable to block network comms");


// Exit callback from probed functions
static int security_hook_exit(struct kretprobe_instance *ri, struct pt_regs *regs);
// Entry callback from the probed functions
static int security_hook_entry(struct kretprobe_instance *ri, struct pt_regs *regs);


// Kernel function hook targets
const char *sendmsg_hook_name = "security_socket_sendmsg";
const char *recvmsg_hook_name = "security_socket_recvmsg";
const char *connect_hook_name = "security_socket_connect";
const char *accept_hook_name  = "security_socket_accept";


// Utility function to initialize a kretprobe data
#define declare_kretprobe(NAME, ENTRY_CALLBACK, EXIT_CALLBACK, DATA_SIZE)   \
static struct kretprobe NAME = {                                            \
    .handler = EXIT_CALLBACK,                                               \
    .entry_handler = ENTRY_CALLBACK,                                        \
    .data_size = DATA_SIZE,                                                 \
    .maxactive = NR_CPUS,                                                   \
};


// Utility function to register a kretprobe with error handling
#define set_kretprobe(KPROBE)                                                   \
do {                                                                            \
    if (register_kretprobe(KPROBE)) {                                           \
        pr_err("MB EDR drv - unable to register a probe\n");                    \
        return -EINVAL;                                                         \
    }                                                                           \
    pr_info("MB EDR drv - register probe for %s\n", *KPROBE.kp.symbol_name);    \
} while (0)


declare_kretprobe(sendmsg_probe, security_hook_entry, security_hook_exit, 0);
declare_kretprobe(recvmsg_probe, security_hook_entry, security_hook_exit, 0);
declare_kretprobe(connect_probe, security_hook_entry, security_hook_exit, 0);
declare_kretprobe(accept_probe, security_hook_entry, security_hook_exit, 0);


static int __init process_network_blocker_init(void) {
    sendmsg_probe.kp.symbol_name = sendmsg_hook_name;
    recvmsg_probe.kp.symbol_name = recvmsg_hook_name;
    connect_probe.kp.symbol_name = connect_hook_name;
    accept_probe.kp.symbol_name  = accept_hook_name;

    pr_info("MB EDR drv - initializing\n");

    set_kretprobe(&sendmsg_probe);
    set_kretprobe(&recvmsg_probe);
    set_kretprobe(&connect_probe);
    set_kretprobe(&accept_probe);

    pr_info("MB EDR drv - initialized\n");
    pr_info("MB EDR drv - targeting process %s\n", executable_path);

    return 0;
}


static void __exit process_network_blocker_exit(void) {
    unregister_kretprobe(&sendmsg_probe);
    unregister_kretprobe(&recvmsg_probe);
    unregister_kretprobe(&connect_probe);
    unregister_kretprobe(&accept_probe);
    pr_info("MB EDR drv - exiting\n");
}


/* Return the file pointer of the executable of a task_struct.
 * The file pointer returned must to be released with fput(file)
 */
static struct file* my_get_task_exe_file(struct task_struct *ctx) {
    struct file *exe_file = NULL;
    struct mm_struct *mm;

    /*
    The `unlikely` macro is a branch prediction hint used to inform the compiler
    that a certain condition is expected to be false most of the time.

    This helps the compiler optimize code layout to improve performance by reducing
    branch mispredictions and improving instruction cache efficiency.
    */
    if (unlikely(!ctx)) {
        return NULL;
    }

    /*
    Spinlock that is used to protect fileds within the locked
    task_struct ctx (process descriptor) from concurrent access.
    */
    task_lock(ctx);
    mm = ctx->mm;

    // PF_KTHREAD is used to identify a kernel thread
    if (mm && !(ctx->flags & PF_KTHREAD)) {
        /* RCU
        There's a whole "book" on RCU: https://docs.kernel.org/RCU/whatisRCU.html

        RCU is a synchronization mechanism added to the linux kernel that is optimized
        for read-mostly situations.

        rcu_read_lock() marks the beginning of an RCU read-side critical section
        */
        rcu_read_lock();

        /*
        The rcu_dereference macro is a fundamental component of the Read-Copy-Update
        (RCU) synchronization mechanism. It is used by a reader thread to safely access
        a pointer that is protected by RCU. Guaranteeing that it sees a consistent
        state of the data structure, even while an updater thread may be concurrently
        modifying it.
        */
        exe_file = rcu_dereference(mm->exe_file);
        /*
        get_file_rcu is a kernel mechanism to safely acquire a reference to a struct file
        pointer in a RCU read-side critical section, primarily for lockless lookups
        of open files.
        */
        if (exe_file && !get_file_rcu(&exe_file)) {
            exe_file = NULL;
        }

        rcu_read_unlock();
    }

    task_unlock(ctx);

    return exe_file;
}


int security_hook_entry(struct kretprobe_instance *ri, struct pt_regs *regs) {
    struct file *fp_executable;
    char *res;
    char exe_path[256];

    memset(exe_path, 0, 256);

    // pr_info("MB EDR drv - security hook entry\n");

    // Get the current task executable file_pointer;
    fp_executable = my_get_task_exe_file(get_current());
    if (!fp_executable) {
        return 1; // Do not call exit handler
    }

    // Gets the path of the fp_executable
    if (IS_ERR(res = d_path(&fp_executable->f_path, exe_path, 256))) {
        pr_info("MB EDR drv - d_path fail\n");
        return 1;
    }

    // If the process executable is the same name, ensure exit callback is executed
    if (!strncmp(res, executable_path, 256)) {
        pr_info("MB EDR drv - Blocking %s\n", res);
        return 0;
    }

    // pr_info("MB EDR drv - Allowing %s\n", res);

    // Return 1: Do not execute the exit calback (security_hook_exit)
    return 1;
}

/* Exit callback:
 * Executed when the probed function returns
 */
int security_hook_exit(struct kretprobe_instance *ri, struct pt_regs *regs) {
    // rax contains the exit value of the probed function
    regs->ax = -EACCES;
    return 0;
}

module_init(process_network_blocker_init);
module_exit(process_network_blocker_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("luigi");

