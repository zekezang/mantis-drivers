#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/delay.h>
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

#include <linux/string.h>

char global_buffer[10];

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

const char *OPEN_KEY = "open";
const char *CLOSE_KEY = "close";

struct proc_dir_entry *example_dir, *hello_file;

int proc_read_hello(char *page, char **start, off_t off, int count, int *eof, void *data) {
	int len;
	len = sprintf(page, global_buffer); //把global_buffer的内容显示给访问者
	return len;
}

int proc_write_hello(struct file *file, const char *buffer, unsigned long count, void *data) {
	int len;
	unsigned val;

	len = (count == 10)? 9 : count;

	memset(global_buffer,0,sizeof(global_buffer));

	if (copy_from_user(global_buffer, buffer, len)) {
		len = -EFAULT;
	}

//	printk(KERN_INFO "------------%d---------%d--------%s---\n",memcmp(OPEN_KEY,global_buffer,strlen(OPEN_KEY)),strlen(OPEN_KEY),global_buffer);

	if(memcmp(OPEN_KEY,global_buffer,strlen(OPEN_KEY)) == 0){
		val = 0x0000;
		writel(val, io_dev.data);
	}else if(memcmp(CLOSE_KEY,global_buffer,strlen(CLOSE_KEY)) == 0){
		val = 0xFFFF;
		writel(val, io_dev.data);
	}else{	}

	return len;
}

unsigned int tmp = 0;
static int __init proc_test_init(void) {
	example_dir = proc_mkdir("gpk13-open", NULL);
	hello_file = create_proc_entry("k", S_IRUGO | S_IWUGO, example_dir);
	strcpy(global_buffer, "hello\n");
	hello_file->read_proc = proc_read_hello;
	hello_file->write_proc = proc_write_hello;

	writel(0x11111111, io_dev.conf0);
	writel(0x11111111, io_dev.conf1);
//	tmp = readl(io_dev.conf1);

	return 0;
}

static void __exit proc_test_exit(void) {
	remove_proc_entry("k", example_dir);
	remove_proc_entry("gpk13-open", NULL);
}

module_init( proc_test_init);
module_exit( proc_test_exit);


