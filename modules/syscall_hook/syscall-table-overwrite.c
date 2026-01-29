#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kallsyms.h>
// #include <linux/moduleparam.h>
#include <linux/unistd.h>
#include <linux/cred.h>
#include <linux/uidgid.h>
#include <linux/version.h>

#include <linux/sched.h>
#include <linux/uaccess.h>

#define MOD_NAME "HOOKER"

#define MAX_FILE_NAME_LENGTH 128

static unsigned long **p_sys_call_table;
// Acquire the system calls table address

// syscall to replace
static char *syscall_sym = "__x64_sys_open";
// module_param(syscall_sym, charp, 0644);


#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,3,0)
static inline void __write_cr0(unsigned long cr0) {
    asm volatile("mov %0,%%cr0" : "+r"(cr0) : : "memory");
}
#else
#define __write_cr0 write_cr0
#endif

// The 16th bit is the Write Protection (WP) bit
static void enable_write_protection(void) {
    unsigned long cr0 = read_cr0();

    set_bit(16, &cr0);
    __write_cr0(cr0);
}


static void disable_write_protection(void) {
    unsigned long cr0 = read_cr0();

    clear_bit(16, &cr0);
    __write_cr0(cr0);
}


/*
 * A pointer to the original system call. The reason we keep this, rather than call
 * the original function (sys_open), is because somebody else might have replaced
 * the system call before us. Note that this is not 100% safe, because if another
 * module replace sys_open before us, then when we are inserted, we will call the
 * function in that module - and it might be removed before we are.
 *
 * Another reason for this is that we cannot get sys_open.
 * It  is a static variable, so it is not exported.
 */
#ifdef CONFIG_ARCH_HAS_SYSCALL_WRAPPER
static asmlinkage long (*original_call)(const struct pt_regs *);
#else
static asmlinkage long (*original_call)(const char __user *, int, umode_t);
#endif

/*
 * The function we will replace sys_open (the function called when you call
 * the open system call) with. To find the exact prototype, with the number and
 * type of arguments, we find the original function first (it is at fs/open.c).
 *
 * In theory, this means that we are tied to the current version of the kernel.
 * In practice, the system calls almost never change (it would wreck havok and
 * require programs to be recompiled, since the system calls are the interface
 * between the kernel and the processes.
 */
#ifdef CONFIG_ARCH_HAS_SYSCALl_WRAPPER
static asmlinkage long our_sys_open(const struct pt_regs *regs)
#else
static asmlinkage long our_sys_open(const char __user *fname, int flags, umode_t mode)
#endif
{
    int ret = 0;
    char ch[MAX_FILE_NAME_LENGTH + 1];

    memset(ch, 0, MAX_FILE_NAME_LENGTH + 1);

#ifdef CONFIG_ARCH_HAS_SYSCALL_WRAPPER
    ret = copy_from_user(ch, (char __user *)regs->si, MAX_FILE_NAME_LENGTH);
#else  // !CONFIG_ARCH_HAS_SYSCALL_WRAPPER
    ret = copy_from_user(ch, (char __user *)fname, MAX_FILE_NAME_LENGTH);
#endif // CONFIG_ARCH_HAS_SYSCALL_WRAPPER
    
    if (ret != 0) {
        pr_alert("[%s] copy_from_user failed\n", MOD_NAME);
        goto orig_func;
    }

    pr_info("[%s] UID %d opened : %s\n", MOD_NAME, __kuid_val(current_uid()), ch);

orig_func:
    // Call the original sys_open - otherwise, we lose the ability
    // top open files.
#ifdef CONFIG_ARCH_HAS_SYSCALL_WRAPPER
    return original_call(regs);
#else   // !CONFIG_ARCH_HAS_SYSCALL_WRAPPER
    return original_call(fname, flags, mode);
#endif  // CONFIG_ARCH_HAS_SYSCALL_WRAPPER
}


static int __init syscall_steal_start(void) {
    p_sys_call_table = (void *) kallsyms_lookup_name("sys_call_table");
    // original_call = (void *)kallsyms_lookup_name(syscall_sym);
    
    if (!p_sys_call_table) {
        pr_alert("[%s] Failed to retrieve syscall table.\n", MOD_NAME);
        return -1;
    }

    original_call = (void *)p_sys_call_table[__NR_open];

    if (!p_sys_call_table || !original_call) {
        // pr_alert("[%s] Missing addresses:\n\tsys_call_table : %p\n\toriginal_call : %p\n", MOD_NAME, p_sys_call_table, original_call);
        return -1;
    }

    pr_info("[%s] Addresses acquired:\n\t\tsys_call_table : %p\n\t\toriginal_call : %p\n\t\tour_call : %p\n", MOD_NAME, p_sys_call_table, original_call, our_sys_open);

    disable_write_protection();
    pr_info("[%s] PRE:  p_sys_call_table[%d] = %p\n", MOD_NAME, __NR_open, p_sys_call_table[__NR_open]);

    // Replace the open function with ours
    p_sys_call_table[__NR_open] = (unsigned long *)our_sys_open;

    pr_info("[%s] POST: p_sys_call_table[%d] = %p\n", MOD_NAME, __NR_open, p_sys_call_table[__NR_open]);
    enable_write_protection();

    pr_info("[%s] %s syscall replaced from %p to %p\n", MOD_NAME, syscall_sym, original_call, p_sys_call_table[__NR_open]);
    return 0;
}


static void __exit syscall_steal_exit(void) {
    if (!p_sys_call_table)
        return;

    if (p_sys_call_table[__NR_open] != (unsigned long *)our_sys_open) {
        pr_alert("[%s] Somebody else also played with the %s system_call\n", MOD_NAME, syscall_sym);
        pr_alert("[%s] System may be left in unstable state...\n", MOD_NAME);
    }

    disable_write_protection();
    p_sys_call_table[__NR_open] = (unsigned long *)original_call;
    enable_write_protection();

    pr_info("[%s] %s syscall restored to %p.\n", MOD_NAME, syscall_sym, p_sys_call_table[__NR_open]);

    msleep(2000);
}

module_init(syscall_steal_start);
module_exit(syscall_steal_exit);

MODULE_LICENSE("GPL");

