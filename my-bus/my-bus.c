/*
 * my-bus.c
 *
 *  Created on: 2012-3-15
 *      Author: zekezang
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>

#include <linux/slab.h>

MODULE_AUTHOR("zekezang");
MODULE_LICENSE("Dual BSD/GPL");

static ssize_t bus_attr_show(struct bus_type *bus, char *buf);
static ssize_t bus_attr_store(struct bus_type *bus, const char *buf, size_t count);

static int my_bus_match(struct device *dev, struct device_driver *drv) {

	printk(KERN_INFO "my_bus_match::111111:%s\n",drv->name);
	printk(KERN_INFO "my_bus_match:::%s::::%s::::%d\n",dev_name(dev),drv->name,strncmp(dev_name(dev), drv->name, strlen(drv->name)));

	return !strncmp(dev_name(dev), drv->name, strlen(drv->name));
}

static struct bus_type my_bus_type = {
		.name = "my-bus",
		.match = my_bus_match, };

EXPORT_SYMBOL(my_bus_type);

static struct bus_attribute my_bus_attribute = {
		.attr = {
				.name = __stringify(version),
				.mode = S_IRWXUGO, },
		.show = bus_attr_show,
		.store = bus_attr_store, };

static char *Version = "$Revision: 1.9 $\n";
static char* zeke_ver;

static ssize_t bus_attr_show(struct bus_type *bus, char *buf) {
	printk(KERN_INFO "bus_attr_show--%s\n",zeke_ver);
	return snprintf(buf, strlen(Version) + 1, Version);
}

static ssize_t bus_attr_store(struct bus_type *bus, const char *buf, size_t count) {
	printk(KERN_INFO "bus_attr_store--%s\n",buf);
	strcpy(zeke_ver, buf);
	return count;
}

static int __init my_bus_init(void) {
	int ret;
	zeke_ver = (char*) kmalloc(8, GFP_KERNEL);
	ret = bus_register(&my_bus_type);
	if (ret)
		return ret;
	ret = bus_create_file(&my_bus_type, &my_bus_attribute);
	if (ret) {
		printk(KERN_INFO "bus_create_file---err\n");
		return ret;
	}

	return ret;
}

static void __exit my_bus_exit(void) {
	kfree(zeke_ver);
	bus_remove_file(&my_bus_type, &my_bus_attribute);
	bus_unregister(&my_bus_type);
}

module_init(my_bus_init);
module_exit(my_bus_exit);
