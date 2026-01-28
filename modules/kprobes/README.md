# kprobes

Kernel probes (kprobes) enables a privileged user to dynamically break into any kernel route and collect debugging/performance information non-disruptively. You can trap nearly all kernel code addresses (some parts of the kernel code are blacklisted.

There are two types of probes:
    - kprobes: can be inserted virtually on any instruction in kernel
    - kretprobes (return probes): fires when a specified function returns

K-probes based instrumentation is packed as a kernel module. The module's init function will register one+ probes while exit function unregisters them.

## How do they function?

It behaves similar to a userland breakpoint insertion. When a kprobe in registered, Kprobes makes a copy of the probed instruction and replaces the first byte with a breakpoint instruction.

When the CPU hits the breakpoint, the CPUs registers are saved and control passes to Kprobes via the notifier\_call\_chain mechanism. Kprobes executes the pre\_handler associated with the kprobe, passing the handler the addresses of the kprobe struct and the saved registers.

Next, Kprobes singe-steps its copy of the probed instruction and execute the `post_handler` (if any) associated with the kprobe. Execution then continues with the instruction following the probepoint.

## Changing execution path

Since Kprobes can probe into running kernel code, it can change the register set, including the instruction pointer. This operation requires maximum care, such as keeping the stack frame, and needs deep knowledge of computer architecture and concurrent computing (aka you can seriously screw things up).

If you change the instruction pointer or other registers in the `pre_handler`, you must return `!0` so that kprobes stop single stepping and just returns to the given address. This also means `post_handler` should not be called anymore.

# Return Probe

## How does it work?

When you call `register_kretprobe()`, Kprobes establishes a kprobe at the entry to the function. When the probe function is called and this probe is hit, Kprobes saves a copy of the return address, and replaces the return address with that of a "trampoline", The trampoline is an arbitrary piece of code - typically just a nop instruction.

When the probed function executes its return instruction, control passes to the trampoline and that probe is hit. Kprobes trampoline handler calls the user-specified return handler associated with the kretprobe, then sets the saved IP to the saved return address, and that's where execution resumes upon return from the trap.

# Caveats

kprobes must be enabled and compiled into the kernel in order to make use of this feature. If it is not configured in the running kernel, you will need to recompile the kernel to make use of kprobes or kretprobes. If it's already configured into the kernel but not enabled, you can simply enable kprobes.

For x86 architectures, the system call table cannot be used to invoke a system call after commit `1e3ad78` since v6.9. This commit has been packported to long term stable kernels, like v5.15.154+, v6.185+, v6.6.26+, and v6.8.5+. In this case, a hook must be used through kprobes to intercept syscalls.

