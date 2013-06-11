#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>

#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <linux/slab.h>

#include "memdev.h"

MODULE_AUTHOR("zekezang");
MODULE_LICENSE("Dual BSD/GPL");

/*文件打开函数*/
int mem_open(struct inode* inode, struct file* filp) {
	struct memdev_struct* dev;
	/*获取次设备号*/
	int num = MINOR(inode->i_rdev);
	if (num >= 1)
		return -ENODEV;
	dev = &(memdev->memdev_struct);

	/*将设备描述结构指针赋值给文件私有数据指针*/
	filp->private_data = dev;

	printk(KERN_INFO "open herer---------------!\n");
	return 0;
}

/*文件释放函数*/
int mem_release(struct inode *inode, struct file *filp) {
	return 0;
}

/*读函数*/
static ssize_t mem_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos) {

	printk(KERN_INFO "read herer---------------!\n");

	unsigned long p = *ppos;
	unsigned int count = size;
	int ret = 0;
	struct memdev_struct *dev = filp->private_data; /*获得设备结构体指针*/

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
	if (p >= mem_size)
		return 0;
	if (count > mem_size - p)
		count = mem_size - p;

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
	return 0;
}

/* seek文件定位函数 */
static loff_t mem_llseek(struct file *filp, loff_t offset, int whence) {
	loff_t newpos;
	return newpos;
}

//ioctl
//long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long)
static long mymdev_ioctl(struct file* filp, unsigned int cmd, unsigned long args) {
	int ret = 0;
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


//主设备号
static int mem_major = 500;
//次设备号
static int mem_minor = 0;
//设备号
dev_t dev_no;

static struct cdev cdev;

static int memdev_probe(struct device *dev) {
	printk(KERN_INFO "Driver found device which memdev driver can handle!\n");
	int ret = 0;
	//获得设备号
	dev_no = MKDEV(mem_major, mem_minor);
	//申请设备号
	if (mem_major) {
		ret = register_chrdev_region(dev_no, 1, "memdev_device");
	} else {
		//动态申请设备号
		ret = alloc_chrdev_region(&dev_no, mem_minor, 1, "memdev_device");
		mem_major = MAJOR(dev_no);
	}
	if (ret < 0) {
		goto err_ret;
	}
	/*初始化cdev结构*/
	cdev_init(&cdev, &mem_fops);
	cdev.owner = THIS_MODULE;
	cdev.ops = &mem_fops;
	//注册字符设备
	cdev_add(&cdev, MKDEV(mem_major, mem_minor), 1);

	//2.6自动创建设备文件
	myclass = class_create(THIS_MODULE, "memdev_device");
	if (IS_ERR(myclass)) {
		goto fail_dev_create;
	}
	device_create(myclass, NULL, MKDEV(mem_major, 0), NULL, "memdev_device");

	memcpy(memdev->memdev_struct.data,"zekezang",8);

	return 0;

	fail_dev_create:
	device_destroy(myclass, MKDEV(mem_major, 0));
	class_destroy(myclass);
	err_ret:
	return ret;
}

static int memdev_remove(struct device *dev) {
	printk(KERN_INFO "Driver found device unpluged!\n");
	class_destroy(dev->class);
	device_del(dev);
	return 0;
}

struct device_driver memdev_driver = {
		.name = "memdev_dev",
		.bus = &memdev_bus_type,
		.probe = memdev_probe,
		.remove = memdev_remove, };

static ssize_t memdevdriver_show(struct device_driver *driver, char *buf) {
	return sprintf(buf, "%s\n", "This is memdev driver!");
}

static DRIVER_ATTR(drv, S_IRUGO, memdevdriver_show, NULL);

static int __init memdev_driver_init(void)
{
	int ret = 0;

	ret = driver_register(&memdev_driver);
	if(ret<0){
		goto err_ret;
	}

	ret = driver_create_file(&memdev_driver, &driver_attr_drv);
	if(ret<0){
		goto err_ret;
	}

	return ret;

	err_ret:
	return -EINVAL;
}

static void __exit memdev_driver_exit(void) {
	driver_remove_file(&memdev_driver, &driver_attr_drv);
	driver_unregister(&memdev_driver);
}

module_init(memdev_driver_init);
module_exit(memdev_driver_exit);
