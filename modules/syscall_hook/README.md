# Syscall Table Overwrite

## Sources

- [lkmpg](https://sysprog21.github.io/lkmpg/)
- [this stack overflow post](https://stackoverflow.com/questions/2103315/linux-kernel-system-call-hooking-example)

## System Calls

Userland processes are able to interact with the kernel through the use of system calls. In general a process is not supposed to be able to access the kernel. It cannot access kernel memory and cannot call kernel function. The hardware of the CPU enforces this through page protection.

The exception to this is system calls. For intel CPUs, processes will fill out its registers with the specific system call id and parameters and then call an `interrupt 0x80` to transition control from the process to the kernel. The hardware knows that once you jump to this location, you are no longer running in restricted user mode, but as the kernel - therefore with system privileges.

The location in the kernel a process can jump to is called a `system_call`. The procedure at the location checks the system call number, which tells the kernel what service the process requested. Then, it looks at the table of system calls (`sys_call_table`) to see the address of the requested kernel function. Then it calls that function, and after it returns, does a few system checks, and returns control back to the process.

So, if we want to change the way a certain system call works, what we need to do is write our own function to implement it and then change the pointer at `sys_call_table` to point to our function. Because we might be removed later and we do not want to leave the system in an unstable state, it's important for `cleanup_module` to restore the table to its original state.

### cr0 Register

To modify the content of `sys_call_table`, we need to consider the control register. A control register is a processor register that changes or controls general behavior of the CPU. For x86, the `cr0` register has various control flags that modify the basic operation of the processor. The `WP` flag in `cr0` stands for write protection. Once the `WP` flag is set, the procesor disallows further write attempts to the read-only sections. Therefore, we must disable the `WP` flag before modifying the `sys_call_table`.

Since Linux v5.3, the `write_cr0` function cannot be used because of the sensitive `cr0` bits pinned by the security issue, the attacker may wrote into CPU control registers to disable CPI protections. A bypass to this is writing custom assembly to modify the `cr0` register.

### Accessing `sys_call_table`

`sys_call_table` is unexported to prevent misue. However, there are numerous techniques to accessing the symbol. This includes manual lookup and using `kallsyms_lookup_name`.
Both these techniques depend on kernel version.

`kallsyms_lookup_name` has been unexported since linux v5.7, requiring a certain in order to get the address. If `CONFIG_KPROBES` is enabled, kprobes can be used to dynamically break into the specific kernal routine we want to hook. This has been demonstrated in the `kprobes` module.

The certain technique for acquiring the `kallsyms_lookup_name` is the following code (no idea on what kernel versions this works on; also untested on my end):

```
    unsigned long int offset = PAGE_OFFSET;  
    unsigned long **sct;
 
  
    while (offset < ULLONG_MAX) {  
        sct = (unsigned long **)offset;  
 
        if (sct[__NR_close] == (unsigned long *)ksys_close)  
            return sct;  
 
        offset += sizeof(void *);  
    }  
  
    return NULL; 
```

Otherwise for manual lookup, specify the address of `sys_call_table` from `/proc/kallsyms` and `/boot/System.map`. 

```
sudo grep sys_call_table /proc/kallsyms
ffffffff82000280 R x32_sys_call_table
ffffffff820013a0 R sys_call_table
ffffffff820023e0 R ia32_sys_call_table
```

When using the address from `/boot/System.map`, be careful about KASLR (kernel addrss space layout randomization). KASLR will randomize the address space of the kernel and data at every boot, so static addresses listed in `/boot/System.map` will be offset by some entropy. Addresses in `/proc/kallsyms` will be correct.

## Risks

If two or more modules mangle with the same system call, this may result in unexpected behavior in the system. There is also risk that if modules are removed in a different order than they were installed, a system call could be restored to a non-existent replacement function of a previously uninstalled kernel module resulting in a system crash.



## Control Flow Integrity

Control flow integrity is a technique used to prevent redirection of execution from an attacker by check that indirect calls go to the expected addresses and return addresses are not changed. Since Linux v5.7, the kernel patched the series of control-flow-enforcement for x86 and some configuration for GCC will added the CET to the kernel by default. However, CET should not be enabled in the kernel as it may break kprobes and bpf. Consequently CET is disabled since v5.11. 

## Caveat

NOTE, **this example has been unavailable since Linux v6.9** due to the system call table changing the implementation from an indirect function call table to a switch statement for security issues, such as Branch History Injection (BHI) attack. **kprobes can be used to hook a system instead** in this scenario, but the kernel must be configured with kprobes and the feature must be enabled.

## DEMO

This code has been demonstrated successfully against the following kernel versions:
- 3.10.18

The code was built using the tools/scripts under the `custom_building` directory in this repo.

The built module was emulated using the tools/scripts under the `emulation` directory in this repo.

Following is the output of commands and `dmesg` demonstrating the successful overwrite of the `open` syscall table entry in the emulated kernel.

```Inside QEMU emulation kernel 3.10.18
[    0.000000] tsc: Fast TSC calibration failed
 ________________________________/\\\________/\\\__/\\\\\\\\\\\\\\\____/\\\\\\\\\______/\\\\\_____/\\\__/\\\\\\\\\\\\\\\__/\\\_____________                                 
  _______________________________\/\\\_____/\\\//__\/\\\///////////___/\\\///////\\\___\/\\\\\\___\/\\\_\/\\\///////////__\/\\\_____________                                
   _______________________________\/\\\__/\\\//_____\/\\\_____________\/\\\_____\/\\\___\/\\\/\\\__\/\\\_\/\\\_____________\/\\\_____________                               
    _______________________________\/\\\\\\//\\\_____\/\\\\\\\\\\\_____\/\\\\\\\\\\\/____\/\\\//\\\_\/\\\_\/\\\\\\\\\\\_____\/\\\_____________                              
     _______________________________\/\\\//_\//\\\____\/\\\///////______\/\\\//////\\\____\/\\\\//\\\\/\\\_\/\\\///////______\/\\\_____________                             
      _______________________________\/\\\____\//\\\___\/\\\_____________\/\\\____\//\\\___\/\\\_\//\\\/\\\_\/\\\_____________\/\\\_____________                            
       _______________________________\/\\\_____\//\\\__\/\\\_____________\/\\\_____\//\\\__\/\\\__\//\\\\\\_\/\\\_____________\/\\\_____________                           
        _______________________________\/\\\______\//\\\_\/\\\\\\\\\\\\\\\_\/\\\______\//\\\_\/\\\___\//\\\\\_\/\\\\\\\\\\\\\\\_\/\\\\\\\\\\\\\\\_                          
         _______________________________\///________\///__\///////////////__\///________\///__\///_____\/////__\///////////////__\///////////////__                         
__/\\\\\\\\\\\\\\\__/\\\\____________/\\\\__/\\\________/\\\__/\\\_________________/\\\\\\\\\_____/\\\\\\\\\\\\\\\__/\\\\\\\\\\\_______/\\\\\_______/\\\\\_____/\\\_        
 _\/\\\///////////__\/\\\\\\________/\\\\\\_\/\\\_______\/\\\_\/\\\_______________/\\\\\\\\\\\\\__\///////\\\/////__\/////\\\///______/\\\///\\\____\/\\\\\\___\/\\\_       
  _\/\\\_____________\/\\\//\\\____/\\\//\\\_\/\\\_______\/\\\_\/\\\______________/\\\/////////\\\_______\/\\\___________\/\\\_______/\\\/__\///\\\__\/\\\/\\\__\/\\\_      
   _\/\\\\\\\\\\\_____\/\\\\///\\\/\\\/_\/\\\_\/\\\_______\/\\\_\/\\\_____________\/\\\_______\/\\\_______\/\\\___________\/\\\______/\\\______\//\\\_\/\\\//\\\_\/\\\_     
    _\/\\\///////______\/\\\__\///\\\/___\/\\\_\/\\\_______\/\\\_\/\\\_____________\/\\\\\\\\\\\\\\\_______\/\\\___________\/\\\_____\/\\\_______\/\\\_\/\\\\//\\\\/\\\_    
     _\/\\\_____________\/\\\____\///_____\/\\\_\/\\\_______\/\\\_\/\\\_____________\/\\\/////////\\\_______\/\\\___________\/\\\_____\//\\\______/\\\__\/\\\_\//\\\/\\\_   
      _\/\\\_____________\/\\\_____________\/\\\_\//\\\______/\\\__\/\\\_____________\/\\\_______\/\\\_______\/\\\___________\/\\\______\///\\\__/\\\____\/\\\__\//\\\\\\_  
       _\/\\\\\\\\\\\\\\\_\/\\\_____________\/\\\__\///\\\\\\\\\/___\/\\\\\\\\\\\\\\\_\/\\\_______\/\\\_______\/\\\________/\\\\\\\\\\\____\///\\\\\/_____\/\\\___\//\\\\\_ 
        _\///////////////__\///______________\///_____\/////////_____\///////////////__\///________\///________\///________\///////////_______\/////_______\///_____\/////__
/ # ls
bin                         proc
dev                         root
etc                         sbin
init                        syscall-table-overwrite.ko
initramfs.cpio.gz           tmp
linuxrc                     usr
/ # insmod syscall-table-overwrite.ko 
/ # cat /etc/motd 
 ________________________________/\\\________/\\\__/\\\\\\\\\\\\\\\____/\\\\\\\\\______/\\\\\_____/\\\__/\\\\\\\\\\\\\\\__/\\\_____________                                 
  _______________________________\/\\\_____/\\\//__\/\\\///////////___/\\\///////\\\___\/\\\\\\___\/\\\_\/\\\///////////__\/\\\_____________                                
   _______________________________\/\\\__/\\\//_____\/\\\_____________\/\\\_____\/\\\___\/\\\/\\\__\/\\\_\/\\\_____________\/\\\_____________                               
    _______________________________\/\\\\\\//\\\_____\/\\\\\\\\\\\_____\/\\\\\\\\\\\/____\/\\\//\\\_\/\\\_\/\\\\\\\\\\\_____\/\\\_____________                              
     _______________________________\/\\\//_\//\\\____\/\\\///////______\/\\\//////\\\____\/\\\\//\\\\/\\\_\/\\\///////______\/\\\_____________                             
      _______________________________\/\\\____\//\\\___\/\\\_____________\/\\\____\//\\\___\/\\\_\//\\\/\\\_\/\\\_____________\/\\\_____________                            
       _______________________________\/\\\_____\//\\\__\/\\\_____________\/\\\_____\//\\\__\/\\\__\//\\\\\\_\/\\\_____________\/\\\_____________                           
        _______________________________\/\\\______\//\\\_\/\\\\\\\\\\\\\\\_\/\\\______\//\\\_\/\\\___\//\\\\\_\/\\\\\\\\\\\\\\\_\/\\\\\\\\\\\\\\\_                          
         _______________________________\///________\///__\///////////////__\///________\///__\///_____\/////__\///////////////__\///////////////__                         
__/\\\\\\\\\\\\\\\__/\\\\____________/\\\\__/\\\________/\\\__/\\\_________________/\\\\\\\\\_____/\\\\\\\\\\\\\\\__/\\\\\\\\\\\_______/\\\\\_______/\\\\\_____/\\\_        
 _\/\\\///////////__\/\\\\\\________/\\\\\\_\/\\\_______\/\\\_\/\\\_______________/\\\\\\\\\\\\\__\///////\\\/////__\/////\\\///______/\\\///\\\____\/\\\\\\___\/\\\_       
  _\/\\\_____________\/\\\//\\\____/\\\//\\\_\/\\\_______\/\\\_\/\\\______________/\\\/////////\\\_______\/\\\___________\/\\\_______/\\\/__\///\\\__\/\\\/\\\__\/\\\_      
   _\/\\\\\\\\\\\_____\/\\\\///\\\/\\\/_\/\\\_\/\\\_______\/\\\_\/\\\_____________\/\\\_______\/\\\_______\/\\\___________\/\\\______/\\\______\//\\\_\/\\\//\\\_\/\\\_     
    _\/\\\///////______\/\\\__\///\\\/___\/\\\_\/\\\_______\/\\\_\/\\\_____________\/\\\\\\\\\\\\\\\_______\/\\\___________\/\\\_____\/\\\_______\/\\\_\/\\\\//\\\\/\\\_    
     _\/\\\_____________\/\\\____\///_____\/\\\_\/\\\_______\/\\\_\/\\\_____________\/\\\/////////\\\_______\/\\\___________\/\\\_____\//\\\______/\\\__\/\\\_\//\\\/\\\_   
      _\/\\\_____________\/\\\_____________\/\\\_\//\\\______/\\\__\/\\\_____________\/\\\_______\/\\\_______\/\\\___________\/\\\______\///\\\__/\\\____\/\\\__\//\\\\\\_  
       _\/\\\\\\\\\\\\\\\_\/\\\_____________\/\\\__\///\\\\\\\\\/___\/\\\\\\\\\\\\\\\_\/\\\_______\/\\\_______\/\\\________/\\\\\\\\\\\____\///\\\\\/_____\/\\\___\//\\\\\_ 
        _\///////////////__\///______________\///_____\/////////_____\///////////////__\///________\///________\///________\///////////_______\/////_______\///_____\/////__
/ # rmmod syscall-table-overwrite.ko 
/ # dmesg
[    9.114257] syscall_table_overwrite: module verification failed: signature and/or required key missing - tainting kernel
[    9.173590] [HOOKER] Addresses acquired:
[    9.173590] 		sys_call_table : ffffffff81801320
[    9.173590] 		original_call : ffffffff811970a0
[    9.173590] 		our_call : ffffffffa0000000
[    9.173804] [HOOKER] PRE:  p_sys_call_table[2] = ffffffff811970a0
[    9.173804] [HOOKER] POST: p_sys_call_table[2] = ffffffffa0000000
[    9.173804] [HOOKER] __x64_sys_open syscall replaced from ffffffff811970a0 to ffffffffa0000000
[    9.185946] [HOOKER] UID 0 opened : /etc/passwd
[   12.998490] [HOOKER] UID 0 opened : /etc/
[   13.551844] [HOOKER] UID 0 opened : /.ash_history
[   13.585681] [HOOKER] UID 0 opened : /etc/motd
[   13.767122] hrtimer: interrupt took 5692554 ns
[   14.009226] [HOOKER] UID 0 opened : /etc/passwd
[   16.956433] [HOOKER] UID 0 opened : .
[   17.625511] [HOOKER] UID 0 opened : /.ash_history
[   17.657793] [HOOKER] __x64_sys_open syscall restored to ffffffff811970a0.
```

