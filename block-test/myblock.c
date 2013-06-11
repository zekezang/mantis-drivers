#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <asm/current.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
/////////////////////////////////////////////////
int kthread_test(void* data);
/////////////////////////////////////////////////
struct semaphore mySema;
struct completion myCompletion;
struct task_struct* myTask;

spinlock_t mySplinlock;

/////////////////////////////////////////////////
static int test_open(struct inode *inode, struct file *filp) {
	printk(KERN_INFO "test open!\n");
	sema_init(&mySema, 1);
	init_completion(&myCompletion);
	myTask = kthread_create(kthread_test, "zekezang", "thest");
	spin_lock_init(&mySplinlock);
	return 0;
}

static long test_ioctl(struct file* filp, unsigned int cmd, unsigned long args) {
	down(&mySema);
//	down_killable(&mySema);
	printk(KERN_INFO "test ioctl!%d\n",current->pid);
	mdelay(3000);
	up(&mySema);

	return 0;
}

static int test_release(struct inode *inode, struct file *filp) {
	printk(KERN_INFO "test release!\n");
	return 0;
}

static ssize_t test_read(struct file *filp, char *buf, size_t count, loff_t *fpos) {
	printk(KERN_INFO "test read!\n");
	wake_up_process(myTask);
	wait_for_completion(&myCompletion);

	printk(KERN_INFO "test read!-completion\n");
	return 0;
}

static ssize_t test_write(struct file *filp, const char *buf, size_t count, loff_t *fpos) {
	spin_lock(&mySplinlock);
	printk(KERN_INFO "test write!\n");

//	mdelay(3000);

	spin_unlock(&mySplinlock);

	return 0;
}

/////////////////////////////////////////////////

int kthread_test(void* data) {

	int num = 0;
	while (num < 5) {

		mdelay(1000);

		printk(KERN_INFO "kthread test function log\n");

		num++;
	}

	complete(&myCompletion);

	return 0;
}

//////////////////////////////////////////////////
struct file_operations fops = {
		.owner = THIS_MODULE,
		.open = test_open,
		.release = test_release,
		.read = test_read,
		.write = test_write,
		.unlocked_ioctl = test_ioctl, };

//////////////////////////////////////////////////
dev_t devno;
struct class *pclass;
struct cdev dev;

int test_dev_init(void) {

	//create a file test-----------------------------------------------




	//create a file test-----------------------------------------------


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
/////////////////////////////////////////////////
/////////////////////////////////////////////////
/////////////////////////////////////////////////
static int __init test_init(void)
{
    printk(KERN_INFO "module test init!\n");
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
/////////////////////////////////////////////////
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("zekezang");
module_init(test_init);
module_exit(test_exit);
/////////////////////////////////////////////////
