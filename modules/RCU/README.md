# RCU - Read Copy Update

## What is RCU?

RCU is a synchronization mechanism that was added to the Linux kernel during 2.5 development that is optimized mainly for read-mostly situations. The basic idea of RCU is to split updates into "removal" and "reclamation" phases.

The removal phases removes references to data items within a data structure (possibly by replacing them with references to new versions of these data items). and can run concurrently with readers. The reason that it is safe to run the removal phase concurrently with readers is the semantics of modern CPUs guarantee thayt readers will see either the old or the new version of the data structure than a partially updated reference.

The reclamation phase odes the work of reclaiming (e.g. freeing) the data tiems removed from the data structure during the removal phase. Because reclaiming data items can disrupt readers concurrently referncing those data items, the reclamation phase must not start until readers no longer hold references to those data items.

Splitting the update into removal and reclamation phases permits the updater to perform the removal phase immediately, and to defer the reclamation phase until all readers active during the removal phases have completed, either by blocking until they finish or by registering a callback that is invoked after they finish. Onlu readers that are active during the removal phase need to be considered, because any reader starting after the removal pase will be unable to gain a reference to the removed data items, and therefore cannot be disrupted by the reclamation phase.

So the typical RCU update sequence goes as follows:

1) Remove pointers to a data structure, so that subsequent readers cannot gain a reference to it
2) Wait for all previous readers to complete their RCU read-side critical sections
3) At this point, there cannot be any readers who hold references to the data structure, so it now may safely be reclaimed (e.g. `kfree()`)

Step 2 above is the key idea underlying RCU's deferred destruction. The ability to wait until all readers are done allows RCU readers to use much lighter-weight syncrhonization, in some case, absolutely no synchronization at all. In contrast, in more conventional lock-based schemes, readers must use heavy-weight syncrhonization in order to prevent an update from deleting the data structure out from under them. This is because lock-based updates typically update data items in place, and must therefore exclude readers. In contrast, RCU-based updaters typically take advtange of the fact that writes to single algined pointers are tomic on modern CPUs, allowing atomic insertion, removal, and replacement of data items in a linked structure without disrupting readers. Concurrent RCU readers can then continue accessing the old version, and can dispense with the atomic operations, memory barriers, and communications cache misses that are so expensive on present-day SMP computer systems, even in absense of lock contention.

In the three-step procedure, the updater is performing both the removal and reclamation step, but it is often helpful for an entirely different thread to do the rclamation, as is in fact the case in the Linux Kernel's directory-entry cache (dcache).

## What is RCU's core API

The core RCU API is:
- `rcu_read_lock()`
- `rcu_read_unlock()`
- `syncrhonize_rcu`/`call_rcu`
- `rcu_assign_pointer()`
- `rcu_dereference()`

### rcu\_read\_lock()

This temporal primitive is used by a reader to inform the reclaimer that the reader is entering an RCU read-side critical section. It is illegal to block while in an RCU read-side critical section, though kernels built with CONFIG\_PREEMPT\_RCU can preempt RCU read-side critical sections. Any RCU-protected data structure accessed during an RCU read-side critical section is guaraneteed to remain unreclaimed for the full durtion of that critical section. Reference counts may be used in conjunction with RCU to maintain longer-term references to data structures.

### rcu\_read\_unlock()

This temporal primitives is used by a reader to inform the reclaimer that the reader is exiting an RCU read-side critical section. Anything that enables bottom halves, preemption, or interrupts also exits an RCU read-side critical section. Releasing a spinlock also exits.

### synchronize\_rcu()

This temporal primitive marks the end of update code and the beginning of reclaimer code. It does this by blocking all pre-existing RCU read-side critical sections on all CPUs have completed. NOTE that `synchronize_rcu()` will not necessarily wait for any subsequent RCU read-side critical sections to complete. For example, consider the following seuqnece of events:

```
        CPU 0                  CPU 1                 CPU 2
    ----------------- ------------------------- ---------------
1.  rcu_read_lock()
2.                    enters synchronize_rcu()
3.                                               rcu_read_lock()
4.  rcu_read_unlock()
5.                     exits synchronize_rcu()
6.                                              rcu_read_unlock()
```

To reiterate, `synchronize_rcu()` waits only for ongoing RCU read-side critical ections to complete, not necessarily for any that begin after.

The `call_rcu()` API is an asynchronous callback form of `synchronize_rcu()`. Instead of blocking, it registers a function and argument which are invoked after all ongoing RCU read-side critical sections have completed. This callback variant is particularly useful in situations where it is illegal to block or where update-side performance is critical.

However, the `call_rcu()` API should not be used lightly, as use of `synchronize_rcu()` results in simpler code. In addition, the `syncrhonize_rcu()` API has the nice propert of automatically limiting the update rate should grave periods by delayed. This property results in system resilience in face of DOS atacks.

### rcu\_assign\_pointer()

The updater uses this spatial macro to assign a new value to an RCU-protected pointer, in order to safely communicate the change in value from the updater to the reader. This is spatial (as opposed to temporal) macro. It does not ealuate to an rvalue, but it does provide any compiler directives and memory-barrier instructions reguired for a given compiler or CPU architecture. Its ordering properties are that of a store-release operation, that is, any prior loads and stores required to initialize the structure are ordered before the store that publishes the pointer to that structure.

Perhaps just as important, `rcu_assign_pointer()` services to document (1) which pointers are protected by RCU and (2) the point at which a given structure becomes accessible to other CPUs.

### rcu\_dereference()

The reader uses this spatial macro to fetch an RCU-protected pointer, which returns a value that may then be safely dereferenced. Note that `rcu_dereference()` does not actually dereference the pointer, instead, it protects the pointer for later dereferencing. IT also executes any needed memory-barrier instructions for a given CPU architecture. Currently only Alpha needs memory barriers within `rcu_dereference()` -- on other CPUs, it compiles to a volatile load.

Common coding practice uses `rcu_dereference()` to copy an RCU-protected pointer to a local variable, then dereferences this local variable, e.g. as follows:

```
p = rcu_dereference(head.next);
return p->data;
```

However, in this case, one could just as easily combine these into one statement:

```
return rcu_dereference(head.next)->data;
```

If you are going to be fetching multiple fields from the RCU-protected structure, using the local variable is of course preferred. Repeated `rcu_dereference()` calls look ugly, do not guarantee that the same pointer will be returned if an update happened while in the critical section, and incur unnecessary overhead on Alpha CPUs.

NOTE that the value returned by `rcu_dereference()` is valid oly within the enclosing RCUb read-side critical section. The following is not legal:

```
rcu_read_lock();
p = rcu_dereference(head.next);
rcu_read_unlock();
x = p->address; /* BUG!!! */
rcu_read_lock();
y = p->data;    /* BUG!!! */
rcu_read_unlock();
```

Holding a reference from one RCU read-side critical section to another is just as illegal as holding a reference from one lock-based critical section to another! Similarly, using a reference outside of the critical section in which it was acquired is just as illegal as doing so with normal locking.

### API Relationship Graph

```
rcu_assign_pointer()
                        +--------+
+---------------------->| reader |---------+
|                       +--------+         |
|                           |              |
|                           |              | Protect:
|                           |              | rcu_read_lock()
|                           |              | rcu_read_unlock()
|        rcu_dereference()  |              |
+---------+                 |              |
| updater |<----------------+              |
+---------+                                V
|                                    +-----------+
+----------------------------------->| reclaimer |
                                     +-----------+
  Defer:
  synchronize_rcu() & call_rcu()
```
