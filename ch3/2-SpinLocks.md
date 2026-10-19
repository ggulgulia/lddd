# Spinlocks

## Main idea
* Hardware based locking primitive
    - i.e. depends on hardware capabilities to provide atomic operations (`test_and_set` for e.g.)
* Works in multicore processors
* Simplest locking primitive
* **Spinlock** is a lock held by CPU.
    - in cotrast to a **mutex** which is a lock held by a task

## How it works
* Consider two CPU's : `CPU-A` and `CPU-B`
    - both are running  `task-A` and `task-B` respectively
    - but `CPU-A` has held the spinlock's lock by calling on the locking fuction
* Now `task-B` calls spinlock's locking function
    - Because the lock is held by `CPU-A` currently executing `task-A`, `CPU-B` will simply *spin* around a *while-loop*
    - This will block the `task-B`, 
        - until `CPU-A` releases the lock by means of `task-A` calling release function on the spin lock
    - Because the waiting on spinlock happens around a *while-loop*, this waiting behaviour  makes it appear as if the CPU is not idle
        - i.e. spinlock burns CPU while waiting
        - in contrast, mutex sleeps the task while waiting making CPU appear idle

## Fine Details of spin lock
* **w.r.t task preemption** : Spin lock disables scheduler and consequently task preemption. 
    - i.e `spin_lock()` prevents preemption of the task currently being executed by CPU holding the spinlock
* **w.r.t interrupts** A task being run by a CPU holding a spinlock can be *interrupted* 
    - when interrupt arrives
        - CPU stops whatever it was doing, while still holding the lock
        - The same CPU runs an interrupt handler on top top of the interrupted code's context
        - when handler has completed, CPU (spinlock holder) returns to continue task exeuction where it left off.
    - A simple `spin_lock()` doesn't prevent interrupt handlers to stop the task temporarily
    - unless the interrupts are disabled on the CPU or disabled locally, i.e. disable until the task has completed
    - the lock holder doesn't deschedule a task because of an interrupt
* Spinning also only makes sense on multiprocessor systems, since the holder must be running on another CPU to release the lock.
    - this makes spinlocks suitable for symmetrical multiprocessing (SMP) safety and for executing atomic task

### Reason for existence of split between spinlock and regular-locks (mutexes)
* A thread that sleeps needs a task context that the scheduler can switch away from and later wake up
* An interrupt handler has not such "task context" and can't sleep since it cannot take a mutex
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
CPU 0, interrupt handler: spin_lock(&lock);       <-tries to get the lock but spins forever
```
* The handler waits for the lock, but the CPU that holds it is the CPU's task the handler interrupted
    - the task can't run until the handler returns
    - the handler cannot return until it acquires the lock
* Nothing on another CPU can fix this, because the holder is stuck on this CPU.

#### Fix for deadlock due to spinlock and IRQ lock acquision attempt: disable local interrupts as well

#### Use functions `spin_unlock_irq` and its complementary locking fuction `spin_lock_irq`
* Function signature:
```c
static void spin_unlock_irq(spinlock_t* lock);
static void spin_lock_irq(spinlock_t* lock);
```
* these functions disable interrupt in addition to disabling preemption

* The `_irq` variant only solves the problem partially
* Consider the following scenario
    - interrupts `x` and `y` are already disabled before the code in the kernel module starts to lock
    - interrupt `z` is enabled before the kernel module starts locking
    - call to `spin_lock_irq(&lock)` will disable the interrupts: Now all three interrupts (`x`, `y` and `z`) are disabled
    - during unlock phase, call to `spin_unlock_irq(&lock)` will dumbly enable all three interrupts, which was not the case before
* This makes `spin_lock_irq` unsafe when colled from IRQ's off-context
* Therefore use `spin_lock_irq` only when it can be guaranteed that the interrupts are enabled and nothing else disabled the interrupts on the local CPU.

#### `spin_lock_irqsave` to solve the issues not handled by `spin_lock_irq`
* `spin_lock_irqsave` saves interrupts status (local to a cpu) in a variable before acquiring lock
* `spin_unlock_irqrestore`, counterpart of above function, restores the status from the saved variable exactly as they were before lock acquisition

```c
//function signatures
static void spin_lock_irqsave(spinlock_t *lock, unsigned long flags);
static void spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags);

//usage
unsigned long flags;

spin_lock_irqsave(&lock, flags);       /* take the lock AND mask IRQs on this CPU */
/* critical section */
...
...
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

### Dynamically creating spinlocks
* For runtime allocation, it's better to embed the spinlock into a bigger structure ( `struct`)
* For the special `struct`, allocate memory and then call `spin_lock_init()` on the spinlock element

```c
struct foo_struct {
    spinlock_t m_lock;
    unsigned int m_foo;
    ...
    ...
};

static struct foo_struct *init_foo_struct()
{
    struct foo_struct *fs;
    fs = kmalloc(sizeof(struct foo_struct), GFP_KERNEL);
    if(!fs)
        return ERR_PTR(-ENOMEM);
    spin_lock_init(&fs->m_lock);
    return fs;
}
```
### Advantages and Disadvantages of static and dynamic creation of spinlocks

#### Static spinlock creation via `DEFINE_SPINLOCK(name)` 
#### Advantages
* **Nothing to forget** : The lock is initialized at compile time, so it is valid before any code runs.
    - no init call to miss
    - no ordering problem with interrupts
    - no ordering problems with other CPUs
* **No init path or error handling**: Spinlock initialization lives in the module's data section -> no allocation that can fail
* **Lockdep works automatically** : Each definition gets its own static lock class
    - deadlock detection (`CONFIG_PROVE_LOCKING`) is accurate with no extra work.
* **Simple lifetime management**: The lock exists as long as the module does, so it can't be freed while somone still uses it.

#### Disadvantages
* **Exactly one lock per kernel module**: It can't be created for each device that a kernel module detects
* **Coarse grained**: If the spinlock protects driver-wide state, every instance of the driver (for eg two cameras) serializes on it even when they touch unrelated data. Contention grows with core count
* **Separate from its data**: The spinlock sits somewhere else in the memory from what it protects
    - worse for code readability
    - worse for cache locality
    - unrelated variables in the same cache line can cause *false sharing*
* **Encourages global state**: Makes the kernel driver hard to extend to multiple devices (for eg 4 sorround view cameras in a car having same camera type and hence requiring one camera driver module)

#### Dynamic: spinlock embedded in a struct plus `spin_lock_init()`
#### Advantages
* **One spinlock per object**: Independent devices or objects don't contend with each other
    - better scalability of the kernel module wrt multiple physical devices
* **Lock sits with the data it protects**: 
    - improves code readability and reasoning about what data the spinlock guards 
    - improves cache locality
    - normal pattern for per-device private data allocated in `probe()`
* **Spinlock lifetime mataches object lifetime**: The lock is created and freed together with the thing it protects
* Works when object count is known at runtime

#### Disadvantages
* **Manual and  correct intialization needed via `spin_lock_init()`**
    - before the object becomes visible to anything else
    - if an interrupt handler is registered or the object is added to a shared list first, another CPU can *take* the uninitialized lock
* **Hidden bug**: with zeroed memory (`kzalloc`), an uninitialized spinlock often look like an unlocked one
    - it appears to work unless lockdep complains ("trying to regisster non static key") or a debug config is enabled
* **Lifetime hazards**: Lock is freed with the object (via `kfree()` for e.g.)
    - It must be ensured that no one is using it when object is `kfree()`-ed
    - use after free on the object is also use after free on the lock
* **Lockdep classes are per init call site, not per object**: 
    - all objects initialized by same `spin_lock_init()` line share one lock class. This is usually what is desired
    - However if taking two locks of same class is desired (locking two objects at once), then `spin_lock_nested()` has to be used, else the lockdep reports a false deadlock
### How to decide which one to chose
| The lock protects...                                                       | Use                                                          |
|----------------------------------------------------------------------------|--------------------------------------------------------------|
| Global or driver-wide state (a global list, a shared counter)              | Static `DEFINE_SPINLOCK`                                     |
| State of one device or object (per-camera registers, per-connection state) | Embedded lock plus `spin_lock_init()` in the allocation path |

