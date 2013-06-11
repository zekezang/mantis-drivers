/*
 *  Bluetooth WIFI state
 *  Copyright (C) 2012  zekezang <xxxxxx>
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/stat.h>
#include <linux/fs.h>
#include <asm/unistd.h>
#include <asm/uaccess.h>
#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/string.h>

char global_buffer[1024];
struct proc_dir_entry *wifi_bt_dir, *wifi_bt_state;
mm_segment_t old_fs;
char* bao = "/data/system/wifi_bt_data";
ssize_t result;
ssize_t ret;
static struct file *file_m_w;
static struct file *file_m_r;

int wifi_bt_proc_read_state(char *page, char **start, off_t off, int count, int *eof, void *data) {
	int len = 0;

	file_m_r = filp_open(bao, O_RDWR, S_IRUGO | S_IWUGO | S_IRGRP | S_IWGRP);

	if (IS_ERR(file_m_r))
		goto open_failed;
	old_fs = get_fs();
	set_fs(get_ds());
	result = file_m_r->f_op->read(file_m_r, global_buffer, 1024, &(file_m_r->f_pos));
	len = snprintf(page, 1024, "%s", global_buffer);
	set_fs(old_fs);
	filp_close(file_m_r, NULL);

	open_failed: {
		len = snprintf(page, 1024, "%s", global_buffer);
	}
	return len;
}

int wifi_bt_proc_write_state(struct file *file, const char *buffer, unsigned long count, void *data) {
	int len;
	if (count == 1024)
		len = 1024 - 1;
	else
		len = count;

	if (copy_from_user(global_buffer, buffer, len)) {
		len = -EFAULT;
	}
	global_buffer[len] = '\0';

	file_m_w = filp_open(bao, O_CREAT | O_RDWR, S_IRUGO | S_IWUGO | S_IRGRP | S_IWGRP);

	if (IS_ERR(file_m_w)) {
		len = -1;
		goto fail_to_open_file;
	}

	old_fs = get_fs();
	set_fs(get_ds());

	ret = file_m_w->f_op->write(file_m_w, global_buffer, strlen(global_buffer), &(file_m_w->f_pos));
	set_fs(old_fs);
	filp_close(file_m_w, NULL);
	fail_to_open_file:

	return len;
}

int __init wifi_bt_proc_state_init(void) {
	wifi_bt_dir = proc_mkdir("wifi_bt_config", NULL);
	wifi_bt_state = create_proc_entry("airplane_signal", S_IRUGO | S_IWUGO | S_IRGRP | S_IWGRP, wifi_bt_dir);
	strcpy(global_buffer, "000\n");
	wifi_bt_state->read_proc = wifi_bt_proc_read_state;
	wifi_bt_state->write_proc = wifi_bt_proc_write_state;
	return 0;
}

void __exit wifi_bt_proc_state_exit(void) {
	remove_proc_entry("airplane_signal", wifi_bt_dir);
	remove_proc_entry("wifi_bt_config", NULL);
}

module_init( wifi_bt_proc_state_init);
module_exit( wifi_bt_proc_state_exit);
