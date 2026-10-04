// SPDX-License-Identifier: GPL-2.0
/*
 * printk_levels_demo.c - prints one message at each kernel log level.
 *
 * pr_fmt() must be defined BEFORE any kernel header is included. It prefixes
 * every pr_*() message with the module name, which makes grepping easy.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

static int repeat = 1;
module_param(repeat, int, 0444);
MODULE_PARM_DESC(repeat, "How many times to print the full set of messages (default 1)");

/*
 * pr_emerg() is level 0. Some systems (rsyslog) broadcast it to every logged-in
 * terminal, so it is opt-in: insmod ... emerg=1
 */
static bool emerg;
module_param(emerg, bool, 0444);
MODULE_PARM_DESC(emerg, "Also print a KERN_EMERG message (default off)");

static int __init printk_levels_demo_init(void)
{
	int i;

	pr_info("module loaded, repeat=%d emerg=%d\n", repeat, emerg);

	for (i = 0; i < repeat; i++) {
		pr_info("--- round %d of %d ---\n", i + 1, repeat);

		if (emerg)
			pr_emerg("level 0: KERN_EMERG   - system is unusable\n");
		pr_alert("level 1: KERN_ALERT   - action must be taken immediately\n");
		pr_crit("level 2: KERN_CRIT    - critical conditions\n");
		pr_err("level 3: KERN_ERR     - error conditions\n");
		pr_warn("level 4: KERN_WARNING - warning conditions\n");
		pr_notice("level 5: KERN_NOTICE  - normal but significant\n");
		pr_info("level 6: KERN_INFO    - informational\n");

		/*
		 * Compiled out unless DEBUG is defined or dynamic debug is
		 * enabled, e.g.: insmod printk_levels_demo.ko dyndbg=+p
		 */
		pr_debug("level 7: KERN_DEBUG   - debug message (round %d)\n", i + 1);
	}

	/* Raw printk with an explicit level prefix (note: no comma after KERN_ERR) */
	printk(KERN_ERR "%s: raw printk(KERN_ERR ...) example\n", KBUILD_MODNAME);

	return 0;
}

static void __exit printk_levels_demo_exit(void)
{
	pr_info("module unloaded\n");
}

module_init(printk_levels_demo_init);
module_exit(printk_levels_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("GG");
MODULE_DESCRIPTION("Demo: printk log levels, pr_debug and module parameters");