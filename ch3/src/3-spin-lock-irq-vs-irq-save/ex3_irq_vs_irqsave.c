// SPDX-License-Identifier: GPL-2.0
/*
 * Exercise 3: spin_lock_irq() vs spin_lock_irqsave() when nested.
 *
 * spin_unlock_irq() ALWAYS re-enables local interrupts. If the caller already
 * had them disabled (here: inside the outer lock_a section), that is a bug.
 * spin_unlock_irqrestore() puts back whatever state was saved.
 *
 * irqs_disabled() tells us the current state of local interrupts.
 *
 * Your tasks are marked TODO 1 and TODO 2.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/irqflags.h>
#include <linux/module.h>
#include <linux/spinlock.h>

static DEFINE_SPINLOCK(lock_a);
static __maybe_unused DEFINE_SPINLOCK(lock_b);	/* used once TODO 1/2 are done */

static void inner_with_irq(void)
{
	/* TODO 1: take lock_b with spin_lock_irq(), release with spin_unlock_irq() */
}

static void inner_with_irqsave(void)
{
	/* TODO 2: take lock_b with spin_lock_irqsave(), release with spin_unlock_irqrestore() */
}

static void run_case(const char *name, void (*inner)(void))
{
	unsigned long flags;

	spin_lock_irqsave(&lock_a, flags);
	pr_info("[%s] holding lock_a:       irqs_disabled=%d (expect 1)\n",
		name, irqs_disabled());

	inner();

	pr_info("[%s] after inner section:  irqs_disabled=%d (expect 1, we still hold lock_a)\n",
		name, irqs_disabled());
	spin_unlock_irqrestore(&lock_a, flags);

	pr_info("[%s] after lock_a release: irqs_disabled=%d (expect 0)\n",
		name, irqs_disabled());
}

static int __init ex3_init(void)
{
	pr_info("start: irqs_disabled=%d\n", irqs_disabled());
	run_case("irq    ", inner_with_irq);
	run_case("irqsave", inner_with_irqsave);
	return 0;
}

static void __exit ex3_exit(void)
{
	pr_info("unloaded\n");
}

module_init(ex3_init);
module_exit(ex3_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Spinlock exercise 3: irq vs irqsave when nested");
