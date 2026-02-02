/*
 * completions.c
 */
#include <linux/completion.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/version.h>

#define MOD_NAME "COMP"

static struct completion crank_comp;
static struct completion flywheel_comp;


static int machine_crank_thread(void *arg) {
    pr_info("[%s] Turn the crank\n", MOD_NAME);
    complete_all(&crank_comp);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,17,0)
    kthread_complete_and_exit(&crank_comp, 0);
#else
    complete_and_exit(&crank_comp, 0);
#endif
}


static int machine_flywheel_spinup_thread(void *arg) {
    wait_for_completion(&crank_comp);

    pr_info("[%s] Flywheel spins up\n", MOD_NAME);

    complete_all(&flywheel_comp);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,17,0)
    kthread_complete_and_exit(&flywheel_comp, 0);
#else
    complete_and_exit(&flywheel_comp, 0);
#endif
}


static int __init completions_init(void) {
    struct task_struct *crank_thread;
    struct task_struct *flywheel_thread;

    pr_info("[%s] Completions example\n", MOD_NAME);

    init_completion(&crank_comp);
    init_completion(&flywheel_comp);

    crank_thread = kthread_create(machine_crank_thread, NULL, "KThread Crank");
    if (IS_ERR(crank_thread))
        goto ERROR_THREAD_1;

    flywheel_thread = kthread_create(machine_flywheel_spinup_thread, NULL, "KThread Flywheel");
    if (IS_ERR(flywheel_thread))
        goto ERROR_THREAD_2;

    wake_up_process(flywheel_thread);
    wake_up_process(crank_thread);

    return 0;

ERROR_THREAD_2:
    kthread_stop(crank_thread);
ERROR_THREAD_1:
    return -1;
}


static void __exit completions_exit(void) {
    wait_for_completion(&crank_comp);
    wait_for_completion(&flywheel_comp);

    pr_info("[%s] Completions exit\n", MOD_NAME);
}

module_init(completions_init);
module_exit(completions_exit);

MODULE_DESCRIPTION("Completions example");
MODULE_LICENSE("GPL");

