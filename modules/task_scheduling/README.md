# Scheduling tasks

There are two main ways of running tasks: tasklets and work queues. Tasklets are a quick and easy way of scheduling a single function to be run (e.g. triggered from an interrupt). Work queues are more complicated but also better suited to running multiple things in a sequence.

## Tasklets

Although tasklet is easy to use, it comes with several drawbacks. Developers have been discussing its removal from the Linux kernel for some time (but prob never happen for compatibility issues). The tasklet callback runs in atomic context, inside a software interrupt, meaning that it cannot sleep or access user-space data, so not all work can be done in a tasklet handler. Also, the kernel only allows one instance of any given tasklet to be running at any given time; multiple different tasklet callbacks can run in parallel.

In recent kernel, tasklets can be replaced by workqueues, timers, or threaded interrupts.

## Work Queues

To add a task to the scheduler, we can use a workqueue. The kernel uses the Completely Fair Scheduler (CFS) to execute work within the queue.

