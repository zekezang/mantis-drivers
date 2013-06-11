/*
 * USB Skeleton driver - 2.2
 *
 * Created on: 2012-7-24
 * Author: zekezang
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/mutex.h>

/* Define these values to match your devices */
#define USB_SKEL_VENDOR_ID	0x093a
#define USB_SKEL_PRODUCT_ID	0x2510

/* table of devices that work with this driver */
static const struct usb_device_id skel_table[] = {
		{
				USB_DEVICE(USB_SKEL_VENDOR_ID, USB_SKEL_PRODUCT_ID) },
		{ } /* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, skel_table);

static int skel_probe(struct usb_interface *interface, const struct usb_device_id *id) {
	struct usb_device* udev = interface_to_usbdev(interface);

	printk(KERN_INFO "zeke info : udev->descriptor.idVendor:%x\n",udev->descriptor.idVendor);
	printk(KERN_INFO "zeke info : udev->descriptor.bDeviceProtocol:%x\n",udev->descriptor.bDeviceProtocol);
	printk(KERN_INFO "zeke info : udev->descriptor.iSerialNumber:%d\n",udev->descriptor.iSerialNumber);

	printk(KERN_INFO "zeke info : udev->devnum:%d\n",udev->devnum);
	printk(KERN_INFO "zeke info : udev->devpath:%s\n",udev->devpath);
	printk(KERN_INFO "zeke info : udev->portnum:%d\n",udev->portnum);
	printk(KERN_INFO "zeke info : udev->product:%s\n",udev->product);
	printk(KERN_INFO "zeke info : udev->manufacturer:%s\n",udev->manufacturer);
	printk(KERN_INFO "zeke info : udev->serial:%s\n",udev->serial);

	return 0;

}

static void skel_disconnect(struct usb_interface *interface) {

	printk(KERN_INFO "zeke info : skel_disconnect\n");


}

static struct usb_driver skel_driver = {
		.name = "zeke_usb",
		.probe = skel_probe,
		.disconnect = skel_disconnect,
		.id_table = skel_table, };

static int __init usb_skel_init(void)
{
	int result;

	result = usb_register(&skel_driver);
	if (result)
		err("usb_register failed. Error number %d", result);

	return result;
}

static void __exit usb_skel_exit(void)
{
	usb_deregister(&skel_driver);
}

module_init(usb_skel_init);
module_exit(usb_skel_exit);

MODULE_LICENSE("GPL");

