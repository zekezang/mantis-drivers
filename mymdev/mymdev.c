/*
 * mymdev.c
 *
 *  Created on: 2012-2-13
 *      Author: zekezang
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <linux/slab.h>

#include <linux/device.h>

#include <linux/delay.h>

#include "mymdev.h"

#define DEBUG    1    /*添加在驱动的程序中,而非头文件*/
#include <linux/platform_device.h>

//字符设备控制信息载体
static struct cdev cdev;

//主设备号
static mym_major = MYMDEV_MAJOR;
//外部参数
//module_param(mym_major, int, S_IRUGO);
//次设备号
static int mym_minor = 0;
//设备号
dev_t dev_no;
//设备
static struct mymdev* mymdev;

static struct class *myclass;

/*文件打开函数*/
int mem_open(struct inode *inode, struct file *filp) {
	struct mymdev* dev;

	/*获取次设备号*/
	int num = MINOR(inode->i_rdev);

	if (num >= MYMDEV_MINOR_COUNT)
		return -ENODEV;
	dev = &mymdev[num];

//	memset(dev->data, 0, dev->size);

	/*将设备描述结构指针赋值给文件私有数据指针*/
	filp->private_data = dev;

	return 0;
}

/*文件释放函数*/
int mem_release(struct inode *inode, struct file *filp) {

	return 0;
}

/*读函数*/
static ssize_t mem_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos) {
	unsigned long p = *ppos;
	unsigned int count = size;
	int ret = 0;
	struct mymdev *dev = filp->private_data; /*获得设备结构体指针*/

	printk(KERN_INFO "dev str:%s \n",dev->data);

	if (down_interruptible(&(dev->sema))) {
		return -ERESTARTSYS;
	}

	while (strlen(dev->data) <= 0) { /*无数据可以读取时*/
		up(&(dev->sema)); //释放当前信号量
		if (filp->f_flags & O_NONBLOCK) {
			return -EAGAIN;
		} //判断是否是非阻塞的
		  //条件不成立 进入休眠等待
		if (wait_event_interruptible(dev->read_queue, (strlen(dev->data) > 0))) {
			return -ERESTARTSYS;
		}

		//wait休眠条件成立，从中退去，将要再次进入while，再次获取信号量
		if (down_interruptible(&(dev->sema))) {
			return -ERESTARTSYS;
		}
	}
	if (p >= MYMDEV_MEM_SIZE)
		return 0;
	if (count > MYMDEV_MEM_SIZE - p)
		count = MYMDEV_MEM_SIZE - p;

	/*读数据到用户空间*/
	if (copy_to_user(buf, (void*) (dev->data + p), count)) {
		ret = -EFAULT;
	} else {
		*ppos += count;
		ret = count;

	}
	printk(KERN_INFO "copy_to_user\n");

	up(&(dev->sema)); //释放当前信号量
	wake_up_interruptible(&(dev->write_queue));
	return ret;
}

/*写函数*/
static ssize_t mem_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos) {
	unsigned long p = *ppos;
	unsigned int count = size;
	int ret = 0;
	struct mymdev *dev = filp->private_data; /*获得设备结构体指针*/

	if (down_interruptible(&(dev->sema_write))) {
		return -ERESTARTSYS;
	}
	/*分析和获取有效的写长度*/
	if (p >= MYMDEV_MEM_SIZE)
		return 0;
	if (count > MYMDEV_MEM_SIZE - p)
		count = MYMDEV_MEM_SIZE - p;

	while (MYMDEV_MEM_SIZE - *ppos <= 0) { /*无空间可以写入*/
		up(&(dev->sema_write)); //释放当前信号量

		if (filp->f_flags & O_NONBLOCK) {
			return -EAGAIN;
		} //判断是否是非阻塞的

		//条件不成立 进入休眠等待
		if (wait_event_interruptible(dev->write_queue, (MYMDEV_MEM_SIZE - *ppos > 0))) {
			return -ERESTARTSYS;
		}

		//wait休眠条件成立，从中退去，将要再次进入while，再次获取信号量
		if (down_interruptible(&(dev->sema_write))) {
			return -ERESTARTSYS;
		}
	}
	/*从用户空间写入数据*/
	if (copy_from_user(dev->data + p, buf, count)) {
		ret = -EFAULT;
	} else {
		*ppos += count;
		ret = count;
	}
	printk(KERN_INFO "write :%s \n",dev->data);

	up(&(dev->sema_write));

	//	msleep(2000); //睡2秒，看效果

	wake_up_interruptible(&(dev->read_queue));
	//	printk(KERN_INFO "dev->data:%s\n",dev->data);
	return ret;
}

/* seek文件定位函数 */
static loff_t mem_llseek(struct file *filp, loff_t offset, int whence) {
	loff_t newpos;

	switch (whence) {
	case 0: /* SEEK_SET */
		newpos = offset;
		break;

	case 1: /* SEEK_CUR */
		newpos = filp->f_pos + offset;
		break;

	case 2: /* SEEK_END */
		newpos = MYMDEV_MEM_SIZE - 1 + offset;
		break;

	default: /* can't happen */
		return -EINVAL;
	}
	if ((newpos < 0) || (newpos > MYMDEV_MEM_SIZE))
		return -EINVAL;

	filp->f_pos = newpos;
	return newpos;
}

//ioctl
//long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long)
static long mymdev_ioctl(struct file* filp, unsigned int cmd, unsigned long args) {
	int ret = 0;
	int arg;

	//检查魔数
	if (_IOC_TYPE(cmd) != MYMDEV_MAGIC_) {
		return -EINVAL;
	}
	//检查基数值是否在预定范围内
	if (_IOC_NR(cmd) > MYMDEV_CMD_SIZE) {
		return -EINVAL;
	}

	switch (cmd) {
	case MYMDEV_UN_PARAM_CMD: {
		printk(KERN_INFO "ioctl....I ------------------------\n");
		break;
	}
	case MYMDEV_W_CMD: {
		get_user(arg, (int*) args);
		printk(KERN_INFO "MYMDEV_W_CMD::%d\n",arg);
		break;
	}
	case MYMDEV_W_CHAR_CMD: {
		char s;
		//access_ok(VERIFY_READ--------);
		get_user(s, (char*) args);
		printk(KERN_INFO "MYMDEV_W_CMD::%c\n",s);
		break;
	}
	case MYMDEV_R_CMD: {
	//check user address is ok use access_ok() function
		//access_ok(VERIFY_WRITE,------);
		put_user(3, (int*) args);
		break;
	}

	default: {
		return -EINVAL;
	}
	}

	return ret;
}

/*文件操作结构体*/
static const struct file_operations mem_fops = {
		.owner = THIS_MODULE,
		.llseek = mem_llseek,
		.read = mem_read,
		.write = mem_write,
		.unlocked_ioctl = mymdev_ioctl,
		.open = mem_open,
		.release = mem_release, };
//设备初始化函数
static int mymdev_init(void) {

	printk(KERN_INFO "zekezang init comming!!!\n");

	int result = 0;

	//获得设备号
	dev_no = MKDEV(mym_major,mym_minor);

	//申请设备号
	if (mym_major) {
		result = register_chrdev_region(dev_no, MYMDEV_MINOR_COUNT, "mymdev");
	} else {
		//动态申请设备号
		result = alloc_chrdev_region(&dev_no, 0, MYMDEV_MINOR_COUNT, "mymdev");
		mym_major = MAJOR(dev_no);
	}

	if (result < 0) {
		return result;
	}

	/*初始化cdev结构*/
	cdev_init(&cdev, &mem_fops);
	cdev.owner = THIS_MODULE;
	cdev.ops = &mem_fops;

	//注册字符设备
	cdev_add(&cdev, MKDEV(mym_major,0), MYMDEV_MINOR_COUNT);

	//设备描述结构分配空间
	mymdev = kmalloc(sizeof(struct mymdev) * MYMDEV_MINOR_COUNT, GFP_KERNEL);
	if (mymdev < 0) {
		result = -ENOMEM;
		goto fail_malloc_mem;
	}

	memset(mymdev, 0, sizeof(struct mymdev) * MYMDEV_MINOR_COUNT);

	/*为设备分配内存*/
	int i;
	for (i = 0; i < MYMDEV_MINOR_COUNT; i++) {

		mymdev[i].size = MYMDEV_MEM_SIZE;
		mymdev[i].data = kmalloc(MYMDEV_MEM_SIZE, GFP_KERNEL);
		memset(mymdev[i].data, 0, MYMDEV_MEM_SIZE);

		init_waitqueue_head(&(mymdev[i].read_queue));
		init_waitqueue_head(&(mymdev[i].write_queue));

		sema_init(&(mymdev[i].sema), 1);
		sema_init(&(mymdev[i].sema_write), 1);
	}

	//2.6自动创建设备文件
	myclass = class_create(THIS_MODULE, "mymdev");
	if (IS_ERR(myclass)) {
		PTR_ERR(myclass);
		goto fail_dev_create;
	}
	device_create(myclass, NULL, MKDEV(mym_major, 0), NULL, "mymdev");

	return 0;

	fail_malloc_mem: unregister_chrdev_region(MKDEV(mym_major,0), 1);

	fail_dev_create: device_destroy(myclass, MKDEV(mym_major, 0));
	class_destroy(myclass);

	return result;
}

/*模块卸载函数*/
static void mymdev_exit(void) {
	printk(KERN_INFO "zekezang exit hohohooho!!!\n");
	device_destroy(myclass, MKDEV(mym_major, 0));
	class_destroy(myclass);
	kfree(mymdev); /*释放设备结构体内存*/
	unregister_chrdev_region(MKDEV(mym_major, 0), 1); /*释放设备号*/
	cdev_del(&cdev); /*注销设备*/
}

MODULE_AUTHOR("zeke zang");
MODULE_LICENSE("GPL");

module_init(mymdev_init);
module_exit(mymdev_exit);

