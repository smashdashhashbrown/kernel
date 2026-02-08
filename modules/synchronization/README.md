# Synchronization

## Mutex

Mutexes in the kernel work much the same as in userspace. Mutexes in the Linux kernel enforce strict ownership: only the task that successfully acquired the mutex can release it. Attempting to release a mutex by another task or releasing an unheld mutex will lead to errors or undefined behavior. If a task tries to lock a mutex it already holds, it may be blocked or sleep, where the task waits for itself to release the lock.

When a task calls `mutex_lock()` and tyhe mutex is unavailable, the task enters into a sleep state until it can successfully obtain the lock. During this period it cannot be interrupted. In conttast, functions with the `_interruptible` suffix, such as `mutex_lock_interruptible()`, allow waiting process to be interrupted by signals. 

`mutex_lock_nested` incorporates the `_nested()` functionality, provided support for nested locking. This prior locking mechanism aids in managing lock acquisition and prevent deadlocks, often employing a subclass parameter for more precise deadlock detection.

Another function is `mutex_trylock()` which attempts to acquire the mutex without bloking. It is generally not safe for use in interrupt context because its implementation isn't atomic. If an interrupt occurs between checking the lock's availability and its acquisition, this can lead to race conditions and potential data corruption.

## Spinlocks

Spinlocks lock up the CPU that the code is running on, taking 100% of the resources. The code within a spinlock block should not take more than a few ms so there is not a noticeable slowdown from the user perspective. 

The example module is `irq safe` in that if interrupts happen during the lock then they will not be forgotten and will activate when the unlock happens, using the flags variable to retain their state.

Taking 100% of a CPUs resources comes with great responsibility. Siutations where the kernel monopolizes a CPU are called *atomic contects*. Holding a spinlock is one of thise siutations. Sleeping in atomic contexts will leave the system hanging, as the occupied CPI devotes 100% of its resources doing nothing but sleeping. Thus, sleeping in atomic contexts is considered a bug in the kernel.

### Different types of spinlock functions

`spin_lock()` does not allow the CPU to sleep while waiting for the lock, making it suitable for most use cases where the critical section is short. However, can be problematic for real-time linux because spinlocks in this configuration behave as sleeping locks. This can prevent other tasks from running and cause the system to become unresponsive. To address this in real-time linux environments, a `raw_spin_lock()` is used, which behaves similarly to a `spin_lock` but without causing the system to sleep.

`spin_lock_irq()` disables interrupts while holding the lock, but it does not save the interrupt state. This means that if an interrupt occurs while the lock is held, the interrupt state could be lost. in contrast, `spin_lock_irqsave()` disables interrupts but saves the interrupt state ensuring that interrupts are restored to their previous state when the lock is released.

`spin_lock_bh()` disables *softirqs* (software interrupts) but also allows hardware interrupts to continue.

## Read/Write Locks

Read and write locks are specialized spinlocks so that you can exclusively read from something or write to something. Like the earlier spinlock examples, `irq` is supported. As before, it is a smart idea to keep anything done within the lock as short as possible to that it does not hang up the system and cause users to start revolting against the tyranny of your module.

## Atomic Operations

If you are doing simple arithmetic, you can use atomic operations to ensure other parts of the system did not mess with them.
