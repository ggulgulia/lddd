// SPDX-License-Identifier: GPL-2.0
/*
 * Exercise 1: protect a shared counter with a spinlock.
 *
 * Several kthreads increment one counter. Without locking, increments are
 * lost (a classic read-modify-write race). The lock is embedded in a
 * dynamically allocated struct, so it needs an explicit spin_lock_init().
 *
 * Your tasks are marked TODO 1 and TODO 2.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/completion.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

static int nthreads = 4;
module_param(nthreads, int, 0444);
MODULE_PARM_DESC(nthreads, "Number of worker threads (default 4)");

static int iters = 200000;
module_param(iters, int, 0444);
MODULE_PARM_DESC(iters, "Increments per thread (default 200000)");

struct shared_state {
	spinlock_t lock;
	unsigned long counter;		/* must only be touched with lock held */
};

static struct shared_state *state;
static struct completion done;

static int worker(void *data)
{
	struct shared_state *s = data;
	int i;

	for (i = 0; i < iters; i++) {
		unsigned long tmp;

		/* TODO 2: take s->lock here */
		tmp = READ_ONCE(s->counter);
		WRITE_ONCE(s->counter, tmp + 1);
		/* TODO 2: release s->lock here */
	}

	/* Safe way for a kthread inside a module to finish and signal done */
	kthread_complete_and_exit(&done, 0);
}

static int __init ex1_init(void)
{
	int i, started = 0;
	unsigned long expected;

	if (nthreads < 1 || iters < 1)
		return -EINVAL;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	/* TODO 1: initialise the spinlock embedded in *state */

	init_completion(&done);

	for (i = 0; i < nthreads; i++) {
		struct task_struct *t;

		t = kthread_run(worker, state, "ex1-worker-%d", i);
		if (IS_ERR(t)) {
			pr_err("kthread_run failed: %ld\n", PTR_ERR(t));
			break;
		}
		started++;
	}

	for (i = 0; i < started; i++)
		wait_for_completion(&done);

	expected = (unsigned long)started * (unsigned long)iters;
	pr_info("threads=%d expected=%lu actual=%lu lost=%lu\n",
		started, expected, state->counter, expected - state->counter);
	return 0;
}

static void __exit ex1_exit(void)
{
	kfree(state);
	pr_info("unloaded\n");
}

module_init(ex1_init);
module_exit(ex1_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Spinlock exercise 1: shared counter");
