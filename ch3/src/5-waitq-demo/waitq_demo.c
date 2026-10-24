// SPDX-License-Identifier: GPL-2.0
/*
 * waitq_demo.c - minimal wait queue example.
 *
 *   read()  on /dev/waitq_demo sleeps until an "event" happens
 *   write() on /dev/waitq_demo makes the event happen and wakes the reader
 *
 * Three things work together:
 *   1. wq         : the wait queue HEAD - a list of sleeping tasks (+ a lock)
 *   2. data_ready : the CONDITION the sleepers wait for (you provide this)
 *   3. wake_up*() : called by the producer AFTER the condition became true
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

/* Defines and intiialzies wq: equivalent ot static wait_queue_head_t wq = ...*/
static DECLARE_WAIT_QUEUE_HEAD(wq);

static bool data_ready = false;

static ssize_t demo_read(struct file* file, char __user * buf, 
						 size_t len, loff_t *ppos)
{
	static const char msg[] = "event happened\n";
	int ret;
	if(len < sizeof(msg) -1)
		return -EINVAL;

	if(!READ_ONCE(data_ready)){
		if(file->f_flags & O_NONBLOCK)
			return -EAGAIN;

		pr_info("pid %d (%s) goes to sleep\n", current->pid, current->comm);

		ret = wait_event_interruptible(wq, READ_ONCE(data_ready));

		if(ret) //in case ret is not zero
			return ret; /* -ERSTARTSYS: interrupted by a signal */
		
		pr_info("pid %d (%s) woke up\n", current->pid, current->comm);
	}

	WRITE_ONCE(data_ready, false); /* what happens if this is skipped or set to true?*/

	if(copy_to_user(buf, msg, sizeof(msg)-1))
		return -EFAULT;

	return sizeof(msg)-1;
}

static ssize_t demo_write(struct file* file, const char __user* buf,
						  size_t len, loff_t *ppos)
{
	WRITE_ONCE(data_ready, true);	/* 1. make the condition true*/
	wake_up_interruptible(&wq);		/* 2. then wake the tasks that are asleep in wq*/

	pr_info("event signalled by pid %d (%s)\n", current->pid, current->comm);
	return len;
}

static const struct file_operations demo_fops = {
	.owner = THIS_MODULE,
	.read = demo_read,
	.write = demo_write,
};

static struct miscdevice demo_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "waitq_demo",
	.fops = &demo_fops,
};

static int __init demo_init(void)
{
	int ret = misc_register(&demo_dev);
	if(ret)
		pr_err("misc_register failed: %d\n", ret);
	else
		pr_info("loaded use /dev/waitq_demo\n");
	
	return ret;
}

static void __exit demo_exit(void)
{
	misc_deregister(&demo_dev);
	pr_info("unloaded\n");
}

module_init(demo_init);
module_exit(demo_exit);

MODULE_LICENSE("GPL-2.0");
MODULE_DESCRIPTION("Minimal demo code associated with lddd repository to showcase wait_queue and wakeup functionalites");
MODULE_AUTHOR("Gajendra");