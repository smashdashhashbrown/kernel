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

    if (unlikely(!ctx)) {
        // pr_info("MB EDR drv - unlikely condition\n");
        return NULL;
    }

    task_lock(ctx);
    mm = ctx->mm;

    if (mm && !(ctx->flags & PF_KTHREAD)) {
        // pr_info("MB EDR drv - PF_KTHREAD condition %p\n", mm);
        rcu_read_lock();

        exe_file = rcu_dereference(mm->exe_file);
        if (exe_file && !get_file_rcu(&exe_file)) {
            // pr_info("MB EDR drv - get_file_rcu condition : %p\n", exe_file);
            exe_file = NULL;
        }
        // pr_info("MB EDR drv - exe_file : %p\n", exe_file);

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

    pr_info("MB EDR drv - Allowing %s\n", res);

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

