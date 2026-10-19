# Spinlock exercises (kernel modules)

Three small modules to practise `spin_lock()`, `spin_lock_irqsave()` and `spin_lock_irq()`.
Each `.c` file has `TODO` markers where you write the locking code. Solutions are in `SOLUTIONS.md`, so try first.

The skeletons and the solutions were compiled against 6.8 kernel headers, but I could not load them into a running kernel. The expected outputs below are what the design should produce, so tell me if your results differ.

## Safety first: use a VM, not your main PC

A kernel module runs with full privileges. The typical mistakes in these exercises have no soft failure:

- **Forgetting `spin_unlock()`** (or returning early while holding the lock): other CPUs spin forever, the module cannot be unloaded, and the whole machine can freeze.
- **A deadlock** (see the stretch exercise): one CPU hangs for good.
- Any bug can take down unsaved work, and a hard reset risks filesystem damage.

So run these in a throwaway Ubuntu VM with **at least 2 vCPUs** (the races need real parallelism) and take a **snapshot** before the first `insmod`.

Options: virt-manager (QEMU/KVM), VirtualBox, or `multipass`. Inside the VM:

```bash
sudo apt install build-essential linux-headers-$(uname -r)
```

The headers must match the VM's running kernel. The code builds on 6.x kernels (it uses `kthread_complete_and_exit()`, available since 5.17).

If you ever run something on your real machine anyway, save everything first and expect that a mistake means a hard reset.

## Build and run

```bash
make
sudo dmesg -C                  # clear the log
sudo dmesg -w -x               # terminal 2: follow the log with levels

sudo insmod ex1_spin_lock.ko   # terminal 1
sudo rmmod ex1_spin_lock
```

Module parameters are passed as `name=value` after the `.ko` file. `modinfo ./ex1_spin_lock.ko` lists them.

---

## Exercise 1: protect a shared counter (`spin_lock`)

Several kernel threads increment one counter 200000 times each. The lock is embedded in a `kzalloc()`ed struct.

1. Build and load it unchanged. `dmesg` shows `expected`, `actual` and `lost`. You should see `lost > 0`, because `counter++` is a read-modify-write that two CPUs can interleave.
2. Try `nthreads=1`. Why is nothing lost now?
3. **TODO 1:** initialise the lock with `spin_lock_init()`.
4. **TODO 2:** take and release the lock around the increment. Rebuild and reload: `lost` must be 0.
5. Questions to answer in your notes:
   - What happens if you do TODO 2 but skip TODO 1? (The memory is zeroed by `kzalloc`, so it may seem to work. Why is that still a bug?)
   - Why is `spin_lock()` (not an `_irq` variant) enough in this exercise? (Hint: does any interrupt handler touch this lock?)
6. Stretch: replace the embedded lock with a global `DEFINE_SPINLOCK()`. Compare the two approaches (discussed earlier: scope of the lock versus scope of the data).

## Exercise 2: lock shared with interrupt context (`irqsave` / `irq`)

A worker thread (process context) and an hrtimer callback (hardirq context) both take `shared_lock`. Both are pinned to the same CPU.

The callback uses `spin_trylock()` as a safety net. Each failed trylock is counted as `WOULD_HAVE_DEADLOCKED`: with a real `spin_lock()` in the callback, that CPU would hang forever.

1. Run it unchanged (`variant=0`, plain `spin_lock()` in the worker):

   ```bash
   sudo insmod ex2_irq_context.ko variant=0
   ```

   `insmod` blocks for about 2 seconds. Expect `WOULD_HAVE_DEADLOCKED` greater than 0: the timer interrupt arrived while the worker held the lock.
2. **TODO 1:** implement `variant=1` with `spin_lock_irqsave()` / `spin_unlock_irqrestore()` around the same critical section. Run with `variant=1`. Expect `WOULD_HAVE_DEADLOCKED=0`, because the interrupt cannot fire on this CPU while the lock is held.
3. **TODO 2:** implement `variant=2` with `spin_lock_irq()` / `spin_unlock_irq()`. Same expectation.
4. Check that `shared_data` equals the expected value in each variant.
5. Experiment with the parameters: `hold_us=10`, `hold_us=150`, `period_us=100`. How does the number of would-be deadlocks change in variant 0, and why?
6. Questions for your notes:
   - Why does the interrupt-disabling version make the callback's `spin_lock()` safe?
   - Why are the worker and the timer pinned to the same CPU? (Hint: if the callback ran on a different CPU and found the lock taken, the holder would just be running elsewhere and would release it soon. Would that still be a deadlock?)

## Exercise 3: `spin_lock_irq` vs `spin_lock_irqsave` when nested

`run_case()` takes `lock_a` with `spin_lock_irqsave()`, calls an inner function that takes `lock_b`, then prints whether local interrupts are disabled (`irqs_disabled()`).

1. **TODO 1:** in `inner_with_irq()`, lock and unlock `lock_b` with `spin_lock_irq()` / `spin_unlock_irq()`.
2. **TODO 2:** in `inner_with_irqsave()`, do the same with `spin_lock_irqsave()` / `spin_unlock_irqrestore()`.
3. Load the module and read the log. For the `irq` case, `irqs_disabled` after the inner section should read 0 even though the outer lock is still held: `spin_unlock_irq()` re-enabled interrupts unconditionally. For the `irqsave` case it stays 1.
4. Questions for your notes:
   - What could go wrong in the `irq` case if an interrupt handler also takes `lock_a`?
   - When is `spin_lock_irq()` the right choice? (Hint: only when you know interrupts are enabled at that point.)

---

## Stretch exercises (VM only)

- **Real deadlock.** In `ex2_irq_context.c`, replace `spin_trylock()` in the timer callback with `spin_lock()` and run `variant=0`. The CPU hangs. Watch the soft-lockup or watchdog messages if your VM is configured to show them, then revert the snapshot. Never do this on a real machine.
- **Softirq variant.** Replace the hrtimer with a `timer_list` (runs in softirq context) and use `spin_lock_bh()` in the worker. What changes compared with `irqsave`?
- **Measure the cost.** Time Exercise 1 with `ktime_get()` for 1, 2 and 4 threads. How does contention scale?

## Quick reference

| Situation | Use |
|---|---|
| Lock never taken from interrupt or softirq context | `spin_lock()` |
| Lock shared with a hardirq handler, IRQ state unknown | `spin_lock_irqsave()` / `spin_unlock_irqrestore()` |
| Lock shared with a hardirq handler, IRQs known to be enabled | `spin_lock_irq()` / `spin_unlock_irq()` |
| Lock shared with a softirq or tasklet | `spin_lock_bh()` / `spin_unlock_bh()` |

Rules that apply to all of them: keep the critical section short, never sleep while holding a spinlock, and always release on every exit path.
