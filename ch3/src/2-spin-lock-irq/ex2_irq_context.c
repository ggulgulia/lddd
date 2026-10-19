// SPDX-License-Identifier: GPL-2.0
/*
 * Exercise 2: a lock shared between process context and interrupt context.
 *
 * A worker kthread (process context) and an hrtimer callback (hardirq
 * context) both use shared_lock. Both are pinned to the SAME CPU, which is
 * exactly the situation where plain spin_lock() in process context deadlocks.
 *
 * SAFETY NET: the timer callback uses spin_trylock() instead of spin_lock().
 * If the trylock fails, the lock can only be held by the code this callback
 * interrupted (same CPU), so the callback counts "would_deadlock" and returns
 * instead of hanging the CPU. In real code you would call spin_lock() there,
 * and every counted event would be a permanent hang.
 *
 * Your tasks are marked TODO 1 and TODO 2.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/version.h>

static int variant;
module_param(variant, int, 0444);
MODULE_PARM_DESC(variant, "0=spin_lock, 1=spin_lock_irqsave, 2=spin_lock_irq (default 0)");

static int run_ms = 2000;
module_param(run_ms, int, 0444);
MODULE_PARM_DESC(run_ms, "How long the test runs, 100..60000 ms (default 2000)");

static int period_us = 200;
module_param(period_us, int, 0444);
MODULE_PARM_DESC(period_us, "Timer period in us, >= 50 (default 200)");

static int hold_us = 50;
module_param(hold_us, int, 0444);
MODULE_PARM_DESC(hold_us, "How long the worker holds the lock per round, in us (default 50)");

static int cpu = -1;
module_param(cpu, int, 0444);
MODULE_PARM_DESC(cpu, "CPU to pin both contexts to (default: first online CPU)");

static DEFINE_SPINLOCK(shared_lock);
static unsigned long shared_data;	/* protected by shared_lock */

/* Only touched by the timer callback (on the pinned CPU) */
static unsigned long timer_runs, timer_got_lock, would_deadlock;

static struct hrtimer timer;
static struct completion done;

static enum hrtimer_restart timer_fn(struct hrtimer *t)
{
	/* hardirq context: local interrupts are already off here */
	timer_runs++;

	if (spin_trylock(&shared_lock)) {
		shared_data++;
		spin_unlock(&shared_lock);
		timer_got_lock++;
	} else {
		would_deadlock++;	/* a real spin_lock() would hang forever */
	}

	hrtimer_forward_now(t, ns_to_ktime((u64)period_us * NSEC_PER_USEC));
	return HRTIMER_RESTART;
}

static int worker(void *unused)
{
	ktime_t deadline = ktime_add_ms(ktime_get(), run_ms);
	unsigned long worker_rounds = 0;
	unsigned long flags __maybe_unused;	/* used once TODO 1 is done */

	/* Started from the pinned worker so the timer lands on this same CPU */
	hrtimer_start(&timer, ns_to_ktime((u64)period_us * NSEC_PER_USEC),
		      HRTIMER_MODE_REL_PINNED);

	while (ktime_before(ktime_get(), deadline)) {
		switch (variant) {
		case 0:
			spin_lock(&shared_lock);
			shared_data++;
			udelay(hold_us);
			spin_unlock(&shared_lock);
			worker_rounds++;
			break;
		case 1:
			/* TODO 1: same critical section using spin_lock_irqsave() */
			break;
		case 2:
			/* TODO 2: same critical section using spin_lock_irq() */
			break;
		}
		cond_resched();
	}

	hrtimer_cancel(&timer);

	pr_info("variant=%d cpu=%d worker_rounds=%lu\n",
		variant, raw_smp_processor_id(), worker_rounds);
	pr_info("timer fired=%lu got_lock=%lu WOULD_HAVE_DEADLOCKED=%lu\n",
		timer_runs, timer_got_lock, would_deadlock);
	pr_info("shared_data=%lu (expected %lu)\n",
		shared_data, worker_rounds + timer_got_lock);

	kthread_complete_and_exit(&done, 0);
}

static int __init ex2_init(void)
{
	struct task_struct *t;
	int target;

	if (variant < 0 || variant > 2 || run_ms < 100 || run_ms > 60000 ||
	    period_us < 50 || hold_us < 1)
		return -EINVAL;

	target = (cpu >= 0 && cpu < nr_cpu_ids && cpu_online(cpu)) ?
		 cpu : cpumask_first(cpu_online_mask);

	init_completion(&done);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 13, 0)
	hrtimer_setup(&timer, timer_fn, CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);
#else
	hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);
	timer.function = timer_fn;
#endif

	t = kthread_create(worker, NULL, "ex2-worker");
	if (IS_ERR(t))
		return PTR_ERR(t);
	kthread_bind(t, target);
	wake_up_process(t);

	/* insmod blocks here for about run_ms */
	wait_for_completion(&done);
	return 0;
}

static void __exit ex2_exit(void)
{
	pr_info("unloaded\n");
}

module_init(ex2_init);
module_exit(ex2_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Spinlock exercise 2: process vs interrupt context");
