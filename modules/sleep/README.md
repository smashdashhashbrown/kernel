# Blocking Processes and Threads

A kernel module that is currently in use can put processes to sleep and wake them up when it is ready for us. This kernel module is an example of this. `sleep.ko` can only be opened by a single process at a time. If the file is already open, the kernel module calls `wait_event_interruptible`. This function changes the status of the task (a task is the kernel data structure which holds information about a process and the system call it is in, if any) to `TASK_INTERRUPTIBLE`, which means that the task will not run until it is woken up somehow, and adds it to `WaitQ`, the queue of tasks waiting to acces the file. Then, the function calls the scheduler to context switch to a different process, one which has some use for the CPU.

When the process is done with the file, it closes it, and `module_close` is called. That function wakes up all the processes in the queue (there's no mechanism to only wake up one of them). It then returns and the process which just closed the file can continue to run. In time, the scheduler decides that that process has had enough and gives control of the CPU to another process. Eventually, one of the processes which was in the queue will be given control of the CPU by the scheduler. It starts at the point right after the call to `WAIT_EVENT_INTERRUPTIBLE`. 

This means that the process is still in kernel mode - as far as the process is concerned, it issued the open system call and the system call has not returned yet. The process does not know somebody else used the CPU for most of the time between the moment it issued the call and the moment it returned.

It can then proceed to set a global variable to tell all the other processes that the file is still open and go on with its life. When the other processes get a piece of the CPU, they'll see that global variable and go back to sleep.

Other signals, such as SIGINT, can also wake up the process. This is because we used `WAIT_EVENT_INTERRUPTIBLE`. We could have used `wait_event` instead, which ignores other signals.

Sometimes processes do not want to sleep. These processes will use the `O_NONBLOCK` flag when opening files. The kernel is supposed to respond by returning with the error code - EAGAIN from operations which would otherwise block.

