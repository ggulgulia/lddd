# Mutex
##  Introduction
* A locking primitive similar to `spin_lock_t`
* With key difference that the code section locked by mutex can sleep
* If a `task-A` tries to lock a mutex held by another `task-B`, `task-A` will find itself suspended
    - Meaning that `task-A` will remain suspended
    - CPU can do something else in this case (in contrast to spin_lock)
* Mutexes is held by a task (in contrast to spinlock held by a CPU)

## Details
* Mutex is a simple data structure that 
    - embeds a wait queue (to put contenders to sleep)
    - a spinlock to protect access to wait queue

* Following snippet illustrates the Mutex strcuture
```c
struct mutex {
    atomic_long_t owner;
    spin_lock_t wait_lock;
#ifdef CONFIG_MUTEX_SPIN_ON_OWNER
    struct optimistic_spin_queue osq; /*spinner MCS lock */
#endif
    struct list_head wait_list;
...
...
};
```
* Following details to note about the `struct mutex`
    - `owner` represents the process/task that owns the lock
    * `wait_list` is the list in which mutex's contenders are put to slip
    * `wait_lock` is the spinlock that protects `wait_list` maipulation
        - i.e removal or insertion of contenders to sleep in it
        - helps to keep `wait_list` coherent on SMP systems
* Mutex API can be found in `include/linux/mutex.h`header
* Prior to acquiring and releasing a mutex, it must be initialized: 
* Similar to spinlocks, mutex can be intialized statically and dynamically

## Static initialization of mutexes
```c
static DEFINE_MUTEX(my_mutex);
``` 

* Definition of `DEFINE_MUTEX()`
```c
#define DEFINE_MUTEX(mutexname) \
struct mutex mutexname = _MUTEX_INITIALIZER(mutexname)
```
## Dynamic initialization of mutexes

* In bigger structs where the data members need to be proteced by mutexes the mutex is usually declared within the struct
* Snippet below shows how to initialize mutex within a data struct
```c
struct struct_with_mutex{
    struct i2c_client* m_client;
    u16* m_reg_config;
    struct mutext m_mutex;
};

static int foo_probe(struct i2c_client* client)
{
    int ret_val{I2C_RET_INIT_SUCCESS};
    ...
    ...
    mutex_init(&data->mutex);
    ...
    ...
    return ret_val;
}
```
* In the above snippet, `mutex_init()` which is a wrapper to a low-level kernel function `__mutex_init()` intializes the mutex.
* Acquiring (or locking) the mutex can be done by calling one of the three api functions:
```c
 void mutex_lock(struct mutex* lock);
 int mutex_lock_interruptible(struct mutex* lock);
 int mutex_lock_killable(struct mutex* lock);
 ```
* With `mutex_lock()`, the task will be put in an uninterruptible sleep state (`TASK_UNINTERRUPTIBLE`) while waiting for the mutex to be released.
* `mutex_lock_interruptible()` will put your task in an interruptible sleep state, in which the sleep can be interrupted by any signal.
* `mutex_lock_killable()` will allow your sleeping task to be interrupted only by signals that actually kill the task.
    - If the `mutex_lock_*` acquires mutex successfully, they return `0`
    - The interruptible and killable variants returns `-EINTR` when the locking attempt was interrupted (or task killed) by a signal.
* Whichever locking function is used, the mutex owner (and only the owner) should release the mutex using `mutex_unlock`:
```c
void mutex_unlock(struct mutex* lock);
```
* It is also possiblee to check the status of mutex whether it is locked or not
```c
static bool mutex_is_locked(struct mutex* lock);
```
This function checks if the mutex `owner` is `NULL` and returns `true` if it is , else `false`
* **NOTE**: `mutex_lock()` should be used 
    - only when it can be guaranteed if the mutex will not be held for a long time
    - if not, then use `mutex_lock_interruptible` vairant instead

## Important points to remember while using mutex
* A mutex can be held by one and only one task at a time
* One held, a mutex must be locked only by the task that locked it
* Multiple, recursive or nested locks are not allowed
* A mutex must be initialized via the API.
    - it must not be initialized by copying
    - it must not be initialized by memset
    - held mutexes must not be reinitialized
* A task that holds a mutex may not exit without freeing up the mutex
* Mutexes may not be used in hardware or software interrupt contexts such as tasklest or timers.

## Where mutex usage makes sense
* Locking only in the user context (what does it mean?)
* If the protected resource is not accessed from IRQ handler and operations need not be atomic

## Where spin lock makes more sense compared to mutex
* In terms of CPU Cycles, cost of using a mutex can be very high due to
    - suspension of current task while waiting for mutex
    - inserting it in wait queue
    - requiring scheduler to switch to another task
    - and rescheduling the sleeping task once mutex is released
* In this case using spinlocks for very small critical sections can be cheaper in terms of CPU load