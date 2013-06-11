#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>

MODULE_AUTHOR("zekezang");
MODULE_LICENSE("Dual BSD/GPL");

extern struct bus_type my_bus_type;

/* Why need this ?*/
static void my_dev_release(struct device *dev) {
printk(KERN_INFO "%s is remove\n",dev_name(dev));
}

struct device my_dev = {
		.bus = &my_bus_type,
		.init_name = "my_dev",
		.release = my_dev_release };

/*
 * Export a simple attribute.
 */
static ssize_t mydev_show(struct device *dev, struct device_attribute *attr, char *buf) {
	printk(KERN_INFO "my-device attr\n");
	return 0;
}

static struct device_attribute dev_attr_device_attr = {
		.attr = {
				.name = __stringify(attr),
				.mode = S_IRWXUGO },
		.show = mydev_show,
		.store = NULL };

static int __init my_device_init(void)
{
	int ret = 0;
        
	ret = device_register(&my_dev);
	if(ret<0){
		goto err_ret;
	}
		
	ret = device_create_file(&my_dev, &dev_attr_device_attr);
	if(ret<0){
		goto err_ret;
	}
	
	return ret;	

	err_ret:
	return -EINVAL;
}

static void __exit my_device_exit(void)
{
	device_remove_file(&my_dev, &dev_attr_device_attr);
	device_unregister(&my_dev);
}

module_init(my_device_init);
module_exit(my_device_exit);
