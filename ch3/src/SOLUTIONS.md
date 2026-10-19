# Solutions

Try the exercises first. These solutions were compiled against 6.8 kernel headers but not loaded into a running kernel.

## Exercise 1: `ex1_spin_lock.c`

TODO 1, in `ex1_init()` right after `kzalloc()`:

```c
spin_lock_init(&state->lock);
```

TODO 2, in `worker()`:

```c
for (i = 0; i < iters; i++) {
	unsigned long tmp;

	spin_lock(&s->lock);
	tmp = READ_ONCE(s->counter);
	WRITE_ONCE(s->counter, tmp + 1);
	spin_unlock(&s->lock);
}
```

Expected result: `lost=0`.

Notes:

- Plain `spin_lock()` is enough because no interrupt handler uses this lock.
- Skipping `spin_lock_init()` often "works" with `kzalloc()`, because all-zero bytes look like an unlocked spinlock on common configs. It is still wrong: debug options (`CONFIG_DEBUG_SPINLOCK`, lockdep) can flag it, and it relies on the lock's internal layout.

## Exercise 2: `ex2_irq_context.c`

TODO 1, `variant=1`:

```c
case 1:
	spin_lock_irqsave(&shared_lock, flags);
	shared_data++;
	udelay(hold_us);
	spin_unlock_irqrestore(&shared_lock, flags);
	worker_rounds++;
	break;
```

TODO 2, `variant=2`:

```c
case 2:
	spin_lock_irq(&shared_lock);
	shared_data++;
	udelay(hold_us);
	spin_unlock_irq(&shared_lock);
	worker_rounds++;
	break;
```

Expected results:

- `variant=0`: `WOULD_HAVE_DEADLOCKED` greater than 0. The timer interrupt fires while the worker holds the lock, on the same CPU.
- `variant=1` and `variant=2`: `WOULD_HAVE_DEADLOCKED=0`. Local interrupts are off while the lock is held, so the callback can only run before the lock is taken or after it is released.
- In all variants `shared_data` equals `worker_rounds + got_lock`.

Notes:

- `spin_lock_irq()` is fine here because the worker runs with interrupts enabled. If the same code could be called with interrupts already off, use `irqsave`.
- The hold time matters: a longer `hold_us` means a larger share of timer interrupts land inside the critical section in variant 0. It also means that in variants 1 and 2 the timer callback is delayed longer, because the interrupt waits until the unlock.
- In real code the callback would call `spin_lock()`, which is safe only because the process-context side disables interrupts.

## Exercise 3: `ex3_irq_vs_irqsave.c`

```c
static void inner_with_irq(void)
{
	spin_lock_irq(&lock_b);
	spin_unlock_irq(&lock_b);
}

static void inner_with_irqsave(void)
{
	unsigned long flags;

	spin_lock_irqsave(&lock_b, flags);
	spin_unlock_irqrestore(&lock_b, flags);
}
```

Expected log pattern:

```
[irq    ] holding lock_a:       irqs_disabled=1
[irq    ] after inner section:  irqs_disabled=0    <- bug: IRQs enabled while lock_a is held
[irq    ] after lock_a release: irqs_disabled=0
[irqsave] holding lock_a:       irqs_disabled=1
[irqsave] after inner section:  irqs_disabled=1    <- correct
[irqsave] after lock_a release: irqs_disabled=0
```

Answers:

- If an interrupt handler also takes `lock_a`, then in the `irq` case it can now fire while the outer code still holds `lock_a`, and it deadlocks on the CPU exactly as in Exercise 2.
- `spin_lock_irq()` is the right choice only when you are certain interrupts are enabled at that point, for example at the top level of a process-context function. In library-style helper code that other callers may call with interrupts off, use `irqsave`.
