#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>

#include <linux/slab.h>

#include "memdev.h"

MODULE_AUTHOR("zekezang");
MODULE_LICENSE("Dual BSD/GPL");

/* Why need this ?*/
static void memdev_dev_release(struct device *dev) {
printk(KERN_INFO "%s is remove\n",dev_name(dev));
	class_destroy(dev->class);
	device_del(dev);
}

/*
 * Export a simple attribute.
 */
static ssize_t memdevdev_show(struct device *dev, struct device_attribute *attr, char *buf) {
	printk(KERN_INFO "memdev-device attr\n");
	return 0;
}

static struct device_attribute memdev_device_attr = {
		.attr = {
				.name = __stringify(attr),
				.mode = S_IRWXUGO },
		.show = memdevdev_show,
		.store = NULL };

struct memdev* memdev;
EXPORT_SYMBOL(memdev);

//字符设备控制信息载体
static struct cdev cdev;

static int __init memdev_device_init(void)
{
	int ret = 0;

	memdev = kmalloc(sizeof(struct memdev), GFP_KERNEL);
	if (memdev < 0) {
		goto err_ret;
	}

	memset(memdev, 0, sizeof(struct memdev));

	memdev->memdev_struct.size = mem_size;
	memdev->memdev_struct.data = kmalloc(mem_size, GFP_KERNEL);
	memset(memdev->memdev_struct.data, 0, mem_size);

	init_waitqueue_head(&(memdev->memdev_struct.read_queue));
	init_waitqueue_head(&(memdev->memdev_struct.write_queue));

	sema_init(&(memdev->memdev_struct.sema), 1);
	sema_init(&(memdev->memdev_struct.sema_write), 1);

	memdev->memdev_dev.bus = &memdev_bus_type;
	memdev->memdev_dev.init_name = "memdev_device";
	memdev->memdev_dev.release = memdev_dev_release;

	ret = device_register(&(memdev->memdev_dev));
	if(ret<0){
		goto err_ret_device_register;
	}

	ret = device_create_file(&(memdev->memdev_dev), &memdev_device_attr);
	if(ret<0){
		goto err_ret_device_create_file;
	}

	return ret;	

	err_ret_device_create_file:
	device_remove_file(&(memdev->memdev_dev), &memdev_device_attr);
	err_ret_device_register:
	device_unregister(&(memdev->memdev_dev));

	kfree(memdev); /*释放设备结构体内存*/
	err_ret:
	return -EINVAL;
}

static void __exit memdev_device_exit(void)
{
	kfree(memdev); /*释放设备结构体内存*/
	device_remove_file(&(memdev->memdev_dev), &memdev_device_attr);
	device_unregister(&(memdev->memdev_dev));
}

module_init(memdev_device_init);
module_exit(memdev_device_exit);
