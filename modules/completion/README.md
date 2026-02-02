# Completions

Completions ate code syncrhonization mechanisms within the kernel that allows tasks to pause execution until the completion of other tasks.

There are the parts to the completion synchronization mechanism:
1) Initialization of struct completion sync pbject
2) The waiting or barrier part through `wait_for_completion()`
3) The signalling side through a call to `complete()`

In this kernel module example, two threads are initiated: (1) crank and (2) flywheel. It is imperative that the crank thread starts before the flywheel thread. A completion state is established for each of these threads, with a dstinct completion defined for both the crank and flywheel threads. At the exit point of each thread, the respective completion state is updated and `wait_for_completion` is used by the flywheel thread to ensure that it does not begin prematurely. The crank thread uses the `complete_all()` function to update the completion, which lets the flywheel thread continue.

So even though the `flywheel_thread` is started first, you should notice when you run `dmesg` that turning the crank always happens first because the flywheel thread waits for the crank thread to complete.

