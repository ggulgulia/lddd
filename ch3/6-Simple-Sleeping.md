# Sleep and Delay mechanisms in kernel

## Simple Sleeping
* *Simple sleeping* can also be referred to as **passive delay**
    - When the task sleeps, the CPU is free to do something else.
    - In contrast to **active delay** where the wait is *busy wait* consuming  CPU Hz while waiting.
    - **NOTE** : `spinlock_t` is *busy wait* mechanism but not *active delay*, which will be discussed shortly.

* The simple sleep API's can be used after including the appropriate header: `#include <linux/delay.h>`
* The following functions can be used for simple sleeping
```c
static inline void usleep_range(unsigned long min, unsigned long max);
static inline void usleep_idle_range(unsigned long min, unsigned long max);
void usleep_range_state(unsigned long min, unsigned long max, unsigned int state);

void msleep(unsigned long msecs);
unsigned long msleep_interruptible(unsigned log msecs);
static inline void ssleep(unsigned int seconds);
static inline void fsleep(unsigned long usecs);
```

* Here's a tabular description of all the functions

| Function                              | Returns         | Behaviour                                                                                                                                                                                          |
|---------------------------------------|-----------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `msleep(msecs)`                       | `void`          | Sleeps at least `msecs` ms, uninterruptible. Signals cannot cut it short.                                                                                                                          |
| `msleep_interruptible(msecs)`         | `unsigned long` | Returns early if a signal arrives. The return value is the **remaining time in ms**, and `0` means it slept the full time. This semantic is from `kernel/time/timer.c`, so verify it in your tree. |
| `usleep_range(min, max)`              | `void`          | Sleeps between `min` and `max` µs, uninterruptible. The window lets the kernel merge your wake-up with other timers.                                                                               |
| `usleep_idle_range(min, max)`         | `void`          | Same, but sleeps in `TASK_IDLE`.                                                                                                                                                                   |
| `usleep_range_state(min, max, state)` | `void`          | The general form: you pass the task state. `usleep_range()` and `usleep_idle_range()` call it.                                                                                                     |
| `ssleep(seconds)`                     | `void`          | `msleep(seconds * 1000)`.                                                                                                                                                                          |
| `fsleep(usecs)`                       | `void`          | Flexible sleep. In 6.8 it calls `udelay()` for up to 10 µs, `usleep_range(usecs, 2 * usecs)` up to 20 ms, and `msleep()` above that.                                                           |

* The `usleep_range()` API relies on high resolution timers (**hrtimers**)
    - it is recommended to use this sleep for few µsecs or small msecs
    - this can avoid the busy-wait loop of `udelay()`


### Task state of when put to sleep with simple sleep API
| Function                              | Task state while asleep                                                                              | `ps` STAT          | Can a signal end it early? | Counts in load average?      |
|---------------------------------------|------------------------------------------------------------------------------------------------------|--------------------|----------------------------|------------------------------|
| `msleep()`, `ssleep()`                | `TASK_UNINTERRUPTIBLE`                                                                               | `D`                | No                         | Yes                          |
| `msleep_interruptible()`              | `TASK_INTERRUPTIBLE`                                                                                 | `S`                | Yes, any signal            | No                           |
| `usleep_range()`                      | `TASK_UNINTERRUPTIBLE`                                                                               | `D`                | No                         | Yes                          |
| `usleep_idle_range()`                 | `TASK_IDLE` (= `TASK_UNINTERRUPTIBLE` + `TASK_NOLOAD`)                                               | `I`                | No                         | No                           |
| `usleep_range_state(min, max, state)` | Whatever `state` you pass                                                                            | Depends on `state` | Depends on `state`         | Depends on `state`           |
| `fsleep()`                            | Up to 10 us: no sleep, stays `TASK_RUNNING`. Up to 20 ms: as `usleep_range()`. Longer: as `msleep()` | `R` or `D`         | No                         | Only while actually sleeping |

### Task state of when put to sleep with delay API
| Function   | Task state                     | `ps` STAT | Gives up the CPU?    | Can a signal end it early? | Notes                                                                                                              |
|------------|--------------------------------|-----------|----------------------|----------------------------|--------------------------------------------------------------------------------------------------------------------|
| `ndelay()` | `TASK_RUNNING` (never changes) | `R`       | No, spins on its CPU | No                         | Resolution is coarser than 1 ns in practice.                                                                       |
| `udelay()` | `TASK_RUNNING` (never changes) | `R`       | No, spins on its CPU | No                         | Interrupts can still run on that CPU unless disabled, which stretches the delay.                                   |
| `mdelay()` | `TASK_RUNNING` (never changes) | `R`       | No, spins on its CPU | No                         | A loop of `udelay(1000)` calls. On a preemptible kernel the scheduler can still switch the task out involuntarily. |


## Kernel delay or busy waiting
* As pointed in the beginning this is active delay mechanism
* Cpu is not idle when one of the busy wait api is used.
* Needs same header to be included to expose the API: `#include <linux/delay.h>`
* Following API's are available
```c
void ndelay(unsigned long nsecs);   /* behaves like this, see below */
void udelay(unsigned long usecs);
void mdelay(unsigned long msecs);
```
* **NOTE**: On some architectures, `udelay` and `mdelay` are macros


## Simple Sleep vs Delay in kernels

|           | Sleeping (`msleep`, `usleep_range`, `ssleep`) | Delaying (`udelay`, `ndelay`, `mdelay`)                            |
|-----------|-----------------------------------------------|--------------------------------------------------------------------|
| Context   | Process context only                          | Any context, including interrupt handlers and with a spinlock held |
| CPU       | Free for other tasks                          | Burned for the whole delay                                         |
| Precision | Coarser (scheduler and timer slack)           | Tighter for very short delays                                      |


## Caveat on `udelay()` , `ndelay()` and `mdelay()`
* These API don’t stop a stopwatch at the requested time.
* They work out how many loop iterations, or timer ticks, should add up to that time, and then spin for that count. 
* If the count is too small, or the spinning is faster than assumed, the call returns before the real time has passed.
s
# Appendix

## All busy wait machanisms in kernel summarized

| Mechanism                                                             | What it waits for                    | Notes                                                                                                                              |
|-----------------------------------------------------------------------|--------------------------------------|------------------------------------------------------------------------------------------------------------------------------------|
| `ndelay()`, `udelay()`, `mdelay()`                                    | A fixed amount of time               | The classic "active delay", for example hardware setup times. `mdelay()` blocks a CPU for milliseconds, so avoid it where you can. On most architectures  `*delay()` are macros |
| Polling loop with `cpu_relax()`                                       | A register bit or variable to change | `while (!(readl(r) & READY)) cpu_relax();`. This has no timeout, so a dead device hangs the CPU.                                   |
| `readl_poll_timeout_atomic()` and related macros (`<linux/iopoll.h>`) | A register condition, with a timeout | A bounded version of the polling loop. Returns `0` or `-ETIMEDOUT`.                                                                |
| Spinlocks (`spin_lock*()`)                                            | Another CPU to release the lock      | Waiting for an event, not for time, but the same technique.                                                                        |
| Bit spinlocks, seqlock read retries                                   | A lock bit or a writer to finish     | Same idea in lighter forms.                                                                                                        |
| Mutex optimistic spinning                                             | The lock owner, for a short while    | Hybrid: spins briefly while the owner is running, then sleeps.                                                                     |