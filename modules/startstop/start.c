/*
 * start.c - Illustration of multi filed modules
 */

#include <linux/kernel.h>
#include <linux/module.h>

int init_module(void) {
    pr_info("Hello world - kernel here speaking.\n");
    return 0;
}

MODULE_LICENSE("GPL");

