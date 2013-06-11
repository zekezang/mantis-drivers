/*
 * my-button.c
 *
 *  Created on: 2012-5-16
 *      Author: zekezang
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/irq.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/regs-gpio.h>

#include <plat/gpio-cfg.h>
#include <mach/gpio-bank-n.h>
#include <mach/gpio-bank-l.h>
#include <linux/wait.h>
#include <linux/slab.h>

#define DEVICE_NAME     "my-button1"

struct button_irq_desc {
	int irq;
	int number;
	char *name;
};

static wait_queue_head_t button_waitq;

static volatile int press = 0;

static struct button_irq_desc bt1 = {
		.irq = IRQ_EINT(1),
		.number = 0,
		.name = "KEY2" };

static irqreturn_t buttons_interrupt(int irq, void *dev_id) {

	unsigned long data;
	int tmp;
//	struct button_irq_desc* b = (struct button_irq_desc*) dev_id;

	mdelay(15);

	data = readl(S3C64XX_GPNDAT);

	tmp = !(data & (1 << 1));

	if (tmp) {
		wake_up_interruptible(&button_waitq);
		press = 1;
	}

//	printk("buttons_interrupt - data:%lu---tmp:%d\n", data, tmp);

	return IRQ_RETVAL(IRQ_HANDLED);
}

static char* rets;

static int my_buttons_open(struct inode *inode, struct file *file) {

	int err = 0;

	err = request_irq(bt1.irq, buttons_interrupt, IRQ_TYPE_EDGE_BOTH, bt1.name, (void*) &bt1);

	if (err) {
		disable_irq(bt1.irq);
		free_irq(bt1.irq, (void *) &bt1);
		return err;
	}

	rets = (char*) kmalloc(sizeof(char) * 1, GFP_KERNEL);
	strcpy(rets, "z");

	return 0;
}

static int my_buttons_close(struct inode *inode, struct file *file) {

	disable_irq(bt1.irq);
	free_irq(bt1.irq, (void *) &bt1);
	return 0;
}

static int my_buttons_read(struct file *filp, char __user *buff, size_t count, loff_t *offp)
{
	int err = 0;

	if (!press) {
		if (filp->f_flags & O_NONBLOCK) {
			return -EAGAIN;
		} else {
			wait_event_interruptible(button_waitq, press);
		}
	}
	press = 0;
	err = copy_to_user(buff, (void *)rets, 1);
	return err;
}

static unsigned int my_buttons_poll(struct file *file, struct poll_table_struct *wait) {
	unsigned int mask = 0;
	poll_wait(file, &button_waitq, wait);

	if (press)
		mask |= POLLIN | POLLRDNORM;
	return mask;
}

static struct file_operations dev_fops = {
		.owner = THIS_MODULE,
		.open = my_buttons_open,
		.release = my_buttons_close,
		.read = my_buttons_read,
		.poll = my_buttons_poll, };

static struct miscdevice misc = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = DEVICE_NAME,
		.fops = &dev_fops, };

static int __init dev_init(void)
{
	int ret;

	init_waitqueue_head(&button_waitq);

	ret = misc_register(&misc);

	printk (DEVICE_NAME" \tinitialized\n");

	return ret;
}

static void __exit dev_exit(void)
{
	misc_deregister(&misc);
	printk("exit\n");
}

module_init( dev_init);
module_exit( dev_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zekezang Inc.");

