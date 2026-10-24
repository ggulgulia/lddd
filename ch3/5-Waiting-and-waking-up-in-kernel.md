# Waiting and waking up mechanisms in kernel

* Term sleeping refers to a mechanism by which a task (on behalf of running kernel code) voluntarily relaxes processor with possibility of another task being scheduled.

* There are two sleeeping mechanisms in linux kernel:
    - **Simple sleeping**
        - Waking up from sleep is implicit after the sleep duration expires
    - **Sleeping based on a conditional event**
        - Another task must explicitly wake up the sleeping task based on a condition
        - Alternatively a sleeping timout can be specified, after which the sleeping task wakes up even if the condition was not set to true
        - **Note**: If no timeout is specified and the condition never became true, then the task sleeps forever

* Fundamentally the difference between the two mechanisms is how the waking up happens:
* **Simple sleeping** in kernel is implemented using a dedicated sleep API.
* **Conditional based sleeping** in kernel is implemented using a wait queue.

## Wait queue
* Kernel scheduler manages a list of tasks to run (tasks in `TASK_RUNNING` state). This list is known as *runqueue*.
    - Tasks can be interruptible having flag `TASK_INTERRUPTIBLE` set.
    - For uninterruptible tasks, the flag `TASK_UNINTERRUPTIBLE` set.
* Sleeping tasks have their own queue called *wait queue*
* Wait queues are higher level mechanisms essentially used for one of the following:
    - process blocking input/output (I/O).
    - to wait for a condition to be true.
    - To wait for a given event to occur.
    - to sense data or resource availability.
* Structure of wait queue defined in `include/linux/wait.h` looks something like:

```c
struct wait_queue_head {
    spinlock_t lock;
    struct list_head head;
};
```
* Thus wait queue data structure consists of the following:
    - a list which consists of sleeping process waiting to be awakend
    - a spinlock to protect access to this list
* Wait queue can therefore be used when more than one process wants to sleep
    - In other words, each process that wants to sleep while waiting for the event to occur puts itself in this list before going to sleep.
    - While a process is in the list, it is called wait queue entry. 
    - When an event occurs, one or more process on the list are woken up and moved off the list.
* Wait queue data structure only remembers which process sleeps in it but doesn't store or examine whether the wake up condition is true or false.
## Two methods of declaring and initializing wait queue

1. **Static initialization**: Use `DECLARE_WAIT_QUEU_HEAD` macro
```c
DECLARE_WAIT_QUEUE_HEAD(wq);
/*equivalent to: wait_queue_head_t wq = some_static_init_steps;
```

2. **Dynamic Initialization**: Use `init_waitqueue_head()`:
```c
wait_queue_head_t wq;

/* wq is embedded in a per device struct */
struct my_dev {
    wait_queue_head_t wq;
    bool data_ready;
};
...
...
/* initialize the wait queue in some other part of the kernel module 
* may be a different function in same file or a different file
*/
init_wait_queue_head(&my_dev->wq);
```
* **NOTE** This has the same  trade-off as `DEFINE_SPINLOCK`and `spint_lock_init()` covered in [Advantages and disadvantages of static vs dynamic spinlock creation](2-SpinLocks.md#advantages-and-disadvantages-of-static-and-dynamic-creation-of-spinlocks)

## Details
### Putting to sleep via `wait_event_*` API calls 
* Any process/task that wants to sleep while waiting for an event to trigger so that the process/task can wake up
* Most of the time the event is availability of a resource or a condition to become true
    - This before going to sleep, it makes sense to check the condition or resource availbility before going to sleeps
* The kernel api that puts the task to sleep also accepts a condition.
*  There are four kernel api's for putting task(s) to sleep:
    - `wait_event(&wq, condition)` : puts the process/task to `TASK_UNINTERRUPTIBLE`
    - `wait_event_interruptible(&wq, condition)` . puts the process/task to `TASK_INTERRUPTIBLE` state
    - `wait_event_killable(&wq, condition)` : puts the task/process to `TASK_KILLABLE` state (not to be discussed here anymore)
    - `wait_event_timeout(&wq, condition, timeout)`: same as `wait_event` but allows for a timout after which the task can wait
    - `wait_event_timeout_interruptible(&wq, condition, timeout)`: same as `wait_event` but allows for a timout after which the task can wait
* The `wait_event_timeout` has two behaviors depenending on the timeout having elapsed or not. These are
    - **Timeout elapsed** : 
        - function returns 0 if `condition` evaluated to `false`
        - function returns 1 if `condition` evaluated to `true`
    - **Timeout not elapsed yet**: Function returns remaining time (in jiffies, at least 1 , more on this later) if the `condition` evaluates to `true`
*  Internally all the flavors of `wait_event` does the following
    - it checks the condition (on resource availability for eg).
    - if `true`, then the task is not put to sleep and a can continue execution.
    - if `false` then the flag `TASK_(UN)INTERRUPTIBLE` flag is set and `schedule()` is called  on the task (which schedules the task for later execution or simply said, the task is put to sleep)
    - here's  a simple code snippet of what happens internally in either of `wait_event_*`function: 
    ```c
    for (;;) {
        prepare_to_wait_event(&wq, &entry, state);   /* add to wait queue, set task state */

        if (condition)  /* condition true, done waiting */
            ret = 0;
            break;

        /* checks only interruptible variants:
        *  after check of condition and then check 
        *  for an interrupt signal 
        */
        if (state == TASK_INTERRUPTIBLE && signal_pending(current)) /* signal arrived, give up waiting */
        {
            ret = -ERESTARTSYS;
            break;
        }

        schedule();                                  /* sleep until somone wakes up the task */
    }
    finish_wait(&wq, &entry);                        /* back to TASK_RUNNING, remove from queue */
    ```
### Waking up a task in sleep state via `wake_up_*` API Calls
* After the change in `condition` or an interrupt (or kill) signal the appropriate `wake_up*` family of function should be called.
    - `wake_up()/wake_up_all()`: wakes up one or all the tasks in the event queue
    - `wake_up_interruptible()/wake_up_interruptible_all()`
* After waking up the task, the task has a corresponding flag set 
    - `TASK_NORMAL` : if the regular. 
* The taks that `wake_up*` family functions can wake up doesn't depend on which `wait_event*` functions were used to put the task to sleep but depends on the state of the task (for eg `TASK_UNINTERRUPTIBLE` vs `TASK_INTERRUPTIBLE` vs `TASK_KILLABLE` etc). 
* Table below summarizes the state of  task set by `wait_event*` function and consequently which `wake_up*` function can be used to wake up the task

| Waiter sleeps with                                                           | Task state                                                 | `wake_up()` / `wake_up_all()` | `wake_up_interruptible()` / `wake_up_interruptible_all()` |
|------------------------------------------------------------------------------|------------------------------------------------------------|-------------------------------|-----------------------------------------------------------|
| `wait_event()`                                                               | `TASK_UNINTERRUPTIBLE`                                     | Yes                           | No                                                        |
| `wait_event_timeout()`, `wait_event_hrtimeout()`                             | `TASK_UNINTERRUPTIBLE`                                     | Yes                           | No                                                        |
| `wait_event_interruptible()`                                                 | `TASK_INTERRUPTIBLE`                                       | Yes                           | Yes                                                       |
| `wait_event_interruptible_timeout()`, `wait_event_interruptible_hrtimeout()` | `TASK_INTERRUPTIBLE`                                       | Yes                           | Yes                                                       |
| `wait_event_freezable()`                                                     | `TASK_INTERRUPTIBLE` (freezable)                           | Yes                           | Yes                                                       |
| `wait_event_killable()`                                                      | `TASK_KILLABLE` (uninterruptible + wakes on fatal signal)  | Yes                           | No                                                        |
| `wait_event_idle()`                                                          | `TASK_IDLE` (uninterruptible, not counted in load average) | Yes                           | No                                                        |

* Hence it can be observed from the table above that 
    - the `wake_up()` and `wake_up_all()` can wake up tasks that are in any one of  `TASK_UNINTERRUPTIBLE` or `TASK_INTERRUPTIBLE` or `TASK_KILLABLE` or `TASK_IDLE` state.
    - the `wake_up_interruptible()` or `wake_up_interruptible_all()` can only wake up tasks that in  `TASK_INTERRUPTIBLE_STATE`

* When the `wake_up*` family functions wakes up the tasks, it updates the states of task again based on which `wake_up*` variant woke the task, described in the table below: 

| Wake function                 | Wakes sleepers in state                          | How many                                               |
|-------------------------------|--------------------------------------------------|--------------------------------------------------------|
| `wake_up()`                   | `TASK_NORMAL` (interruptible or uninterruptible) | All non-exclusive waiters + at most 1 exclusive waiter |
| `wake_up_all()`               | `TASK_NORMAL`                                    | All waiters, exclusive ones included                   |
| `wake_up_interruptible()`     | `TASK_INTERRUPTIBLE` only                        | All non-exclusive waiters + at most 1 exclusive waiter |
| `wake_up_interruptible_all()` | `TASK_INTERRUPTIBLE` only                        | All waiters, exclusive ones included                   |

* For the interrup
### Fine Details

* `wait_event_interruptible` doesn’t allow interruption of the task in any destructive sense. 
* It puts the task to sleep in a state where it can be woken up, either by means of `condition`becoming true or by a signal. That’s what “interruptible” means.
    - When the `condition` is true, the `wait_event_interruptible`
* Consider the snippet
```c
static DECLARE_WAIT_QUEUE_HEAD(my_event);   /* the wait queue head */
static bool data_ready;                     /* the condition */

/* reader */
ret = wait_event_interruptible(my_event, READ_ONCE(data_ready));
if (ret)
    return ret;                             /* -ERESTARTSYS: signal arrived */

/* writer / producer */
WRITE_ONCE(data_ready, true);               /* 1. condition first */
wake_up_interruptible(&my_event);           /* 2. then wake sleepers */
```
 - combining it with the simplified snippet from the `wait_event*` above , following can happen when a task is under sleep quue and/or woken up by `wake_up*` or a signal:

 | Waiter                       | Situation                                                                                           | Blocked or returns?                                                  | What the wait call returns            |
|------------------------------|-----------------------------------------------------------------------------------------------------|----------------------------------------------------------------------|---------------------------------------|
| `wait_event_interruptible()` | `data_ready` is already `true` when the call runs                                                   | Returns immediately. The task never sleeps and no wake-up is needed. | `0`                                   |
| `wait_event_interruptible()` | `data_ready` is `false`                                                                             | Blocks. The task sleeps until woken.                                 | Nothing yet (still inside the call)   |
| `wait_event_interruptible()` | `wake_up_interruptible()`, `wake_up()` or `wake_up_all()` arrives and `data_ready` is `true`        | Returns. Re-checks the condition and finds it true.                  | `0`                                   |
| `wait_event_interruptible()` | `wake_up_interruptible()`, `wake_up()` or `wake_up_all()` arrives but `data_ready` is still `false` | Stays blocked. Re-checks, finds it false, and goes back to sleep.    | Nothing yet (still inside the call)   |
| `wait_event_interruptible()` | A signal arrives (for example Ctrl+C) while the condition is false                                  | Returns, with no wake-up involved.                                   | `-ERESTARTSYS`                        |
| `wait_event()`               | `data_ready` is already `true` when the call runs                                                   | Returns immediately. The task never sleeps and no wake-up is needed. | No value (it is a `void`-style macro) |
| `wait_event()`               | `data_ready` is `false`                                                                             | Blocks. The task sleeps until woken.                                 | Nothing yet (still inside the call)   |
| `wait_event()`               | `wake_up()` or `wake_up_all()` arrives and `data_ready` is `true`                                   | Returns. Re-checks the condition and finds it true.                  | No value (it is a `void`-style macro) |
| `wait_event()`               | `wake_up()` or `wake_up_all()` arrives but `data_ready` is still `false`                            | Stays blocked. Re-checks, finds it false, and goes back to sleep.    | Nothing yet (still inside the call)   |
| `wait_event()`               | `wake_up_interruptible()` or `wake_up_interruptible_all()` arrives, even with `data_ready` `true`   | Stays blocked. An uninterruptible sleeper is not woken by these.     | Nothing yet (still inside the call)   |
| `wait_event()`               | A signal arrives (for example Ctrl+C)                                                               | Stays blocked. It cannot be interrupted by signals.                  | Nothing yet (still inside the call)   |