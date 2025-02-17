# Introduction

Loadable Kernel Modules (LKM): Executable library that extends the capabilities of a running kernel of an OS. Can add kernel modules during runtime, but can also be abused by hackers.

# Basics

## What are LKMs

LKMs are loadable kernel modules usd by the Linux kernel to expand functionality. Allows dynamic loading without need for recompilation of whole kernel.

Every LKM consist of two basic functions:

```
int init_module(void) /*used for all initialition stuff*/
{
...
}

void cleanup_module(void) /*used for a clean shutdown*/
{
...
}
```

To load a module:
`insmod module.o`
This command forces the system to do the following:
- Load the objectfile
- call `create_module` systemcall
- unresolved references are resolved by kernel-symbols with the systemcall `get_kernel_syms`
- After this `init_module` systemcall is used for the LKM init, executing the `int init_module(void)` function

Example LKM:

```
#define MODULE
#include <Linux/module.h>

int init_module(void)
{
 printk("<1>Hello World\n");
 return 0;
}

void cleanup_module(void)
{
 printk("<1>Bye, Bye");
}
```

Commands
```
# Compile and run with:
gcc -c -O3 helloworld.c
insmod helloworld.o
# List with
lsmod
# Remove module with
rmmod helloworld
```

## What are systemcalls

System calls: functions built into kernel that represent transition from user to kernel space. Each systemcall has a defined number. Kernel uses `interrupt 0x80` for managing every systemcall.

Systemcall number is an index in an array of a kernel structure called `sys_call_table[]` which maps the numbers to the needed service function.

## What is the Kernel-Symbol-Table

`/proc/ksyms`: Every entry in the file respresents an exported Kernel Symbol, which can be accessed by our LKM. Can be used to our advantage.

Every symbol used in our LKM is also exported to the public and is also listed in that file. Potential for an admin to discover. So how to fix? LKM developers are able to use the following oiece of regular code to limit the exported symbols of their module:

```
static struct symbol_table module_syms= { // We define our own symbol table
    #include <linux/symtab_begin.h>       // Symbols we want to export, do we??
};

// register_symtab(&module_syms); // If we wanted to make symbols public
register_symtab(null); // Construction used to prevent export of any symbols to public
// This line must be inserted in the `init_module()` function
```

## How to transform Kernel to User Space Memory

Systemcalls get their arguments from user space, but our LKM runs in kernel space. How can we access an argument allocated in user space from our kernel space module? Solution: make a *transition*.

The article mentions the user of a kernel mode function for retrieving user space memory bytes:

```
#include <asm/segment.h>

get_user(pointer);
```

Giving this function a pointer to our *path location helps us get the bytes from user space to kernel space. Look at the implementation made bu plaguez for moving strings from user to kernel space:

```
char *strncpy_fromfs(char *dest, const char *src, int n)
{
    char *tmp = src;
    int compt = 0;

    do {
	dest[compt++] = __get_user(tmp++, 1);
    }
    while ((dest[compt - 1] != '\0') && (compt != n));

    return dest;
}
```

For normal data transitions, the following function is the easiest way of doing:

```
#include <asm/segment.h>
void memcpy_fromfs(void *to, const void *from, unsigned long count);
```

Now how do we convert from kernel space to user space? And how to allocate user space memory from our kernel space position?

For the conversions, use the following:
```
#include <asm/segment.h>
void memcpy_tofs(void *to, const void *from, unsigned long count);
```

But for the allocation of user space for the `*to` pointer, we can use the following trick:
```

/*we need brk systemcall*/
static inline _syscall1(int, brk, void *, end_data_segment);

...

int ret, tmp;
char *truc = OLDEXEC;
char *nouveau = NEWEXEC;
unsigned long mmm;

mmm = current->mm->brk;
ret = brk((void *) (mmm + 256));
if (ret < 0)
   return ret;
memcpy_tofs((void *) (mmm + 2), nouveau, strlen(nouveau) + 1);
```

`current` is a pointer to the task structure of the current process. `mm` is the pointer to the `m_struct` responsible for the memory management of that process. By using the `brk` systemcall on `current->mm->brk` we are able to increase the size of the unused area of the datasegment which can be used for copying kernel space memory to user space of the current process.

## Ways to use user space like functions

As you saw in I.4, we used a syscall macro for our own brk call, which if like the one we know from user space. 

Another way of implementing functions:
```
int (*open)(char *, int, int); /*declare a prototype*/

open = sys_call_table[SYS_open];  /*you can also use __NR_open*/
```

Just use the function pointer from the `sys_call_table`. Be careful when supplying arguments for those systemcalls, they need them in user space not from your kernel space position.

A very easy way of doing this is playing with the needed registers. You have to know that Linux uses segment selectors to differentiate between kernel space and user space. Arguments used with systemcalls which were issued from user space are somewhere in the data segment selector (DS) range. DS can be retrived by using `get_ds()` from `asm/segment.h`. So data used as parameters by systemcalls can only be accessed from kernel space if we set the segment selector used for the user segment by the kernel to the needed DS value. This can be done by using `set_fs(...)`. *But be careful*, you have to restore FS ater you accessed the argument of the systemcall. So lets look at a code fragment showing something useful:

```
->filename is in our kernel space; a string we just created, for example

unsigned long old_fs_value=get_fs();

set_fs(get_ds);               /*after this we can access the user space data*/
open(filename, O_CREAT|O_RDWR|O_EXCL, 0640);
set_fs(old_fs_value);         /*restore fs...*/
```

## List of daily needed Kernelspace Functions
