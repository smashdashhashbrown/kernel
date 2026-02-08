/*
 * mutex.c
 */
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/printk.h>

#define MOD_NAME "mutex"

static DEFINE_MUTEX(mymutex);

static int __init example_mutex_init(void) {
    int ret;

    pr_info("[%s] mutex init\n", MOD_NAME);

    ret = mutex_trylock(&mymutex);
    if (ret != 0) {
        pr_info("[%s] mutex is locked\n", MOD_NAME);

        if (mutex_is_locked(&mymutex) == 0)
            pr_info("[%s] The mustex failed to lock!\n", MOD_NAME);
        mutex_unlock(&mymutex);

        pr_info("[%s] mutex is unlocked\n", MOD_NAME);
    } else {
        pr_info("[%s] failed to lock\n", MOD_NAME);
    }

    return 0;
}


static void __exit example_mutex_exit(void) {
    pr_info("[%s] mutex exit\n", MOD_NAME);
}


module_init(example_mutex_init);
module_exit(example_mutex_exit);

MODULE_DESCRIPTION("Mutex example");
MODULE_LICENSE("GPL");

