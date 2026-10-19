# Trylock methods

## Introduction
* There might be situation where the lock (spinlock or mutex) needs to be acqiured only if it not already held by another contener
* If the lock is not available, then waiting is not desired
    - i.e spinlock will not spin if the lock is held by another CPU
    - in case of mutex, the task will not sleep if the mutex is held by another task.
* The kernel provides the following:
    - `spin_trylock(struct spin_lock_t* lock)`
    - `mutex_trylock(struct mutex* lock)`
* The semantics of the `*_trylock` functions allow them to be used with if-conditions
    - either early return if the lock could not be acquired.
    - or conditionally execute another set of code if the lock is not available.

## Code Snippet
* For `spin_trylock`:

```c
static DEFINE_SPINLOCK(foo_lock);
...
...

static void foo(void)
{
    ...
    ...

    if(!spin_trylock(&foo_lock)){
        ...
        return;
    }
    /**
     * reaching this part of code means that
     *  spinlock was acquired
     */
    ...
    ...
    spin_unlock(&foo_lock);
    ...
    ...
    return;
}
```

* For `mutex_trylock()`:
```c
static DEFINE_MUTEX(bar_mutex);
...
...
static void bar(void)
{
    ...
    ...
    if(!mutex_trylock(&bar_mutex))
    {
        ...
        ...
        return;
    }
    /**
     * reaching this part of code means that
     * mutex has been acquired
     */
    ...
    ...
    mutex_unlock(&bar_mutex);
    ...
    ...
    return;
}
```