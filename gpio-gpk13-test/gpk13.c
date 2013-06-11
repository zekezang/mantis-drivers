#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <asm/current.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <linux/gpio.h>

#include <asm/io.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/regs-gpio.h>

#include <plat/gpio-cfg.h>
#include <mach/gpio-bank-e.h>
#include <mach/gpio-bank-k.h>
static int use_count = 0;

int kthread_test(void* data);
struct completion myCompletion;
struct task_struct* myTask;

spinlock_t mySplinlock;

struct tiny6410_gpio_dev
{
 unsigned *conf0;
 unsigned *conf1;
 unsigned *data;
 unsigned *pud;
};

struct tiny6410_gpio_dev io_dev=
{
 .conf0=S3C64XX_GPKCON,
 .conf1=S3C64XX_GPKCON1,
 .data=S3C64XX_GPKDAT,
 .pud=S3C64XX_GPKPUD,
};

unsigned int tmp = 0;

/////////////////////////////////////////////////
static int test_open(struct inode *inode, struct file *filp) {
	printk(KERN_INFO "test open!\n");
	spin_lock(&mySplinlock);
	use_count ++;
	if(use_count > 1){
		use_count --;
		return -1;
	}
	spin_unlock(&mySplinlock);
	init_completion(&myCompletion);

	writel(0x11111111, io_dev.conf0);
	tmp = readl(io_dev.conf0);
	printk(KERN_INFO "-------------------------------------S3C64XX_GPKCON:%x\n",tmp);


	writel(0x11111111, io_dev.conf1);
	tmp = readl(io_dev.conf1);
	printk(KERN_INFO "-------------------------------------S3C64XX_GPKCON1:%x\n",tmp);



	return 0;
}

static long test_ioctl(struct file* filp, unsigned int cmd, unsigned long args) {
	printk(KERN_INFO "test ioctl!%d\n",current->pid);

	myTask = kthread_create(kthread_test, "zekezang", "thest");
	wake_up_process(myTask);

	wait_for_completion_interruptible(&myCompletion);

	return 0;
}

static int test_release(struct inode *inode, struct file *filp) {
	printk(KERN_INFO "test release!\n");
	use_count = 0;
	return 0;
}


int kthread_test(void* data) {

	unsigned val;
	int k = 0;
	while(k<150){
		k++;
		if(k%2){
//			val = readl(io_dev.data);
//			printk(KERN_INFO "-------------------------------------\n");
			val = 0xFFFF;
//			printk(KERN_INFO "raw val:%x\r\n",val);
			writel(val, io_dev.data);
		}else{
//			val = readl(io_dev.data);
//			printk(KERN_INFO "-------------------------------------\n");
			val = 0x0000;
//			printk(KERN_INFO "raw val:%x\r\n",val);
			writel(val, io_dev.data);
		}
		mdelay(100);
	}

	writel(0xFFFF, io_dev.data);

	complete_and_exit(&myCompletion,1);

	return 0;
}

//////////////////////////////////////////////////
struct file_operations fops = {
		.owner = THIS_MODULE,
		.open = test_open,
		.release = test_release,
		.unlocked_ioctl = test_ioctl, };

//////////////////////////////////////////////////
dev_t devno;
struct class *pclass;
struct cdev dev;

int test_dev_init(void) {
	int result;
	result = alloc_chrdev_region(&devno, 0, 1, "test001");
	if (result != 0) {
		printk(KERN_INFO "alloc_chrdev_region failed!\n");
		goto ERR1;
	}

	cdev_init(&dev, &fops);
	dev.owner = THIS_MODULE;
	dev.ops = &fops;
	result = cdev_add(&dev, devno, 1);
	if (result != 0) {
		printk(KERN_INFO "cdev_add failed!\n");
		goto ERR2;
	}

	pclass = class_create(THIS_MODULE, "test001");
	if (IS_ERR(pclass)) {
		printk(KERN_INFO "class_create failed!\n");
		goto ERR3;
	}
	device_create(pclass, NULL, devno, NULL, "test001");

	return 0;
	ERR3: cdev_del(&dev);
	ERR2: unregister_chrdev_region(devno, 1);
	ERR1: return -1;
}

static int __init test_init(void)
{
    printk(KERN_INFO "module test init!\n");
    use_count = 0;
    spin_lock_init(&mySplinlock);
    return test_dev_init();
}

static void __exit test_exit(void)
{
    printk(KERN_INFO "module test exit!\n");
    device_destroy(pclass,devno);
    class_destroy(pclass);
    unregister_chrdev_region(devno, 1);
    cdev_del(&dev);
}
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("zekezang");
module_init(test_init);
module_exit(test_exit);
