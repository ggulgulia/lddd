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
* Two commonly used synchronization mechanisms: 
    - **Locks**  for mutual exlusion: When one contender holds lock, no other can hold it, i.e. others are excluded
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
    - hardware performs read, compare and write as one indivisible step
    - consequently no other CPU can see or change the value of `lock` halfway through
    - Common examples of hardware supported atomic operations
        - **test-and-set**
        - **compare-and-swap**
        - **atomic add**
    - if two CPU's attempt the atomic operation simultaneously, hardwae orders (order as in "arrange" and not order as in "command") them and exactly one CPU wins the contetion
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
    - A task finishing, for e.g. a **DMA aka Direct memory access** transfer completing a transaction 

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
* Both mechanisms make the waiting taks **sleep** instead of spinning, so the CPU is free for other task
* **Use a wait queue** when the wake up depends on a **Condition you check** (flag set or list not empty)
* **Use completion mechanism** for a simple **this thing has finished doing what it was supposed to** signal
* Waiters can sleep, so they must run in process context.  Waking (`wake_up`, `complete`) is safe from interrupt handlers [need more explaination].