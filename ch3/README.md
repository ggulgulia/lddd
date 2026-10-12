# Introduction
* Kernel is a standalone piece of software
    - it doesn't depend on external libraries
    - any functionality it needs is implemented from scratch
        - list management, thread libaries, compression algorithms etc to name a few
* This chapter covers 
    - linux kernel locking mechanisms
    - dealing with sleeping, waiting, and delay
    - linux kernel time management
    - work-deferring mechanisms
    - kernel interrupt handling

# Kernel locking mechanism and shared resources

* A  resource is said to be shared when it is accessible by several contenders, whether exclusively or not
    - Exclusive : access must be synchronized so that only allowed cotenders my own resource
    - In this case:
        - **Resources** maybe: memory locations, peripheral devices
        - **Contenders** may be: processors, processes or threads
* OS achieves mutual exclusion by
    - atomically modifying variables that holds current state of resource
    - making the atomic state visible to all contenders that might access the resource simultaneously
* Modern OS nowadays rely on hardware to implement synchronization
* Two commonly used syncrhonization mechanisms: 
    - **Locks**  for mutual exlusion: When one contender holds lock, on other can hold it, i.e. others are excluded
        - Commonly available locks are : **Spinlocks**, **Semaphores** and **Mutexes**
    - **Conditioanl Variables**: For wating for a change to happen over the shared resources.

## Exclusive ownership of resources

* For locking mechanisms, kernel relies on hardware support for lock based synchronization by means of atomic operations

* Consider the following example

  ```c
      if (lock == 0) {   /* step 1: read and check lock */
          lock = 1;      /* step 2: acquire lock        */
          /* critical code section */
      }
  ```

    - With two CPUs, both can execute step 1 before either can run step 2.
    - In this case both see `lock == 0`.
    - both set `lock = 1` and start modifying the critical sections
    - thus locking did not work 
* In **atomic operation** supported by hardware
    - hardware perfrom read, compare and write as one divisible step
    - consequently no other CPU can see or change the value of `lock`halfway through
    - Common examples of hardware supported atomic operations
        - **test-and-set**
        - **compare-and-swap**
        - **atomic add**
    - if two CPU's attempt the atomic operation simultaneously, hardwae oders them and exaclty one CPU wins the contetion
* In ARM64
    - Cortex-A has 
        - **Load-Exclusive/Store-Exclusive** (`LDXR`/`STXR`) (ARM-v8.0)
        - **Load-Acquire-Exclusive/Store-Acquire-Exclusive** (`LDAXR`/`STAXR`) (ARMV-8.0)
        - **LSE atomics**: single instrucitons such as LDADD, SWP and CAS that do the whole read-modify-write in one go (ARM-v8.1)
            - LSE atomics scale better under contentions with many cores
    - Kernel abstracts hardware specific implementaitons of locks behind APIs like `atomic_t`, `atomic_inc()`, `cmpxchg()`

## Waiting for resource to change (*wait queues* and *completions*)

### Main idea
* In contrast to exlusive ownerships via locks, sometimes a task needs to **wait for the resource's state to change** instead.
* Linux kernel has not condition variables (the `pthread_cond_t` kind from user space). It provides its own mechanisim instead

### Examples fo wait for a state change
* A list going from empty to not empty 
    - i.e. wait until the list holds at least one object
    - A task finishing, for eg a **DMA aka Direct memory access** transfer completing a transaction 

### Kernel mechanisms
| Mechanism | Used to wait for | Notes |
|---|---|---|
| **Wait queue** | A change in state or condition | Designed to work together with locks |
| **Completion queue** | A given computation to complete | Mostly used with DMA |

### Quick API sketch
```c
/*wait queue: wait until a contidion becomes true*/
DECLARE_WAIT_QUEUE_HEAD(wq);
static bool data_ready;

/* waiter sleeps here*/
wait_event_interruptible(wq, data_ready);

/* producer */
data_ready = true;
wake_up_interruptible(&wq);

/*completion : wait until someone signals "done"*/
DECLARE_COMPLETION(done);

/* waiter stops here*/
wait_for_completion(&done);

/*consumer, eg. from DMA-done IRQ handler */
complete(&done);
```
### Key points
* Both mechanisms make thw waiting taks **sleep** instead of spinning, so the CPU is free for other task
* **Use a wait queue** when the wake up depends on a **Condition you check** (flag set or list not empty)
* **Use completion mechanism** for a simple **this thing has finished doing what it was supposed to** signal
* Waiters can sleep, so they must run in process context.  Waking (`wake_up`, `complete`) is safe from interrupt handlers.

# Spinlocks

## Main idea
* Hardware based locking primitive
    - i.e. depends on hardware capabilities to provide atomic operations (`test_and_set` for e.g.)
* Works in multicore processors
* Simplest locking pritive
* **Spinlock** is a lock held by CPU 
    - in cotrast to a **mutex** which is a lock held by a task

## How it works
* Consider two CPU's : `CPU-A` and `CPU-B`
    - both are running  `task-A` and `task-B` respectively
    - but `CPU-B` has held the spinlock's lock by calling on the locking fuction
* Now `task-B` calls spinlock's locking function
    - Because the lock is held by `task-A`, `CPU-B` will simply *spin* around a *while-loop*
    - This will block the `task-B`, 
        - until `CPU-B` releases the lock by calling release function on the spin lock
    - Because the waiting on spinlock happens around a *while-loop*, this waiting behaviour  makes it appear as if the CPU is not idle
        - i.e. spinlock burns CPU while waiting
        - in contrast, mutex sleeps the task while waiting making CPU appear idle

## Fine Details of spin lock
* **w.r.t task preemption** : Spin lock disables scheduler and consequently task preemption. 
    - i.e `spin_lock()` prevents preemption
* **w.r.t interrupts** A task being run by a CPU holding a spinlock can be *interrupted* 
    - when interrupt arrives
        - CPU stops whatever it was doing
        - runs an interrupt handler
        - when handler has completed, it returns to continue exeuction where it left off
    - A simple `spin_lock()` doesn't prevent interrupt handlers to stop the task temporarily
    - unless the interrupts are disabled on the CPU or disabled locally, i.e disable until the task has completed
    - the lock holder doesn't deschedule a task because of an interrupt
* Spinning also only makes sense on multiprocessor systems, since the holder must be running on another CPU to release the lock.
    - this makes spinlocks suitable for symmetrical multiprocessing (SMP) safety and for executing atomic task

### Reason for existence of split between spinlock and regular-locks (mutexes)
* A thread that sleeps needs a task context that the scheduler can switch away from and later wake up
* An interrupt handler has not such "task context" and can't sleep si it cannot take a mutext
    - A spinlock is the only choice for an interrupt handler
* Sleeping while hlding a spinlock is a bug
    - because another CPU may spin on that lock indefinitely
    - if this happens, in this case the kernel will warn that "sleeping function called from invalid context"

### Potential lock-deadlock with spinlock and IRQs
* If the interrupt handler tries to take the same lock, the CPU deadlocks:
* Consider the scenario

```
CPU 0, process context:   spin_lock(&lock);       <- holds the lock
CPU 0, interrupt arrives: handler runs
CPU 0, handler:           spin_lock(&lock);       <-tries to get the lock but spins forever
```
* The handler waits for the lock, but the task that holds it is the task the handler interrupted
    - the task can't run until the handler returns
    - the handler cannot return until it acquires the lock
* Nothing on another CPU can fix this, because the holder is stuck on this CPU.

#### Fix for deadlock due to spinlock and IRQ lock acquision attempt: disable local interrupts as well
```c
unsigned long flags;

spin_lock_irqsave(&lock, flags);       /* take the lock AND mask IRQs on this CPU */
/* critical section */
spin_unlock_irqrestore(&lock, flags);  /* release and restore previous IRQ state */
```
Variants:

| Variant                                 | Disables                                                         | Use when                                                                                     |
|-----------------------------------------|------------------------------------------------------------------|----------------------------------------------------------------------------------------------|
| `spin_lock()`                           | preemption only                                                  | lock is never taken from interrupt or softirq context                                        |
| `spin_lock_irqsave()` / `spin_lock_irqrestore()` | preemption and local IRQs, saving the old state                  | lock is shared with an interrupt handler, and you may not know whether IRQs were already off |
| `spin_lock_irq()` / `spin_unlock_irq()` | preemption and local IRQs, unconditionally re-enabling at unlock | you know IRQs are enabled at that point                                                      |
| `spin_lock_bh()`                        | preemption and softirqs/tasklets                                 | lock is shared with softirq or tasklet code                                                  |

On ARM64, masking IRQs means setting the interrupt mask bit in the processor state (`DAIF`).

### Creating and intializing spinlocks

#### Statically creating spinlocks
* using the `DEFINE_SPINLOCK`macro
```c
#include <linux/spinlock_types.h>

static DEFINE_SPINLOCK(foo_lock);
```
* The macro is defined in `include/linux/spinlock_types.h` as follows:
```c
#define DEFINE_SPINLOCK(x) spinlock_t x = __SPIN_LOCK_UNLOCKED(x)
```

* After the spinlock has been initialized by above macro, it is accessible through its name `foo_lock` nad its address would be `&foo_lock`