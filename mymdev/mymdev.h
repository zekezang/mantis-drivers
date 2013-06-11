/*
 * mymdev.h
 *
 *  Created on: 2012-2-13
 *      Author: zekezang
 */

#include <asm/ioctl.h>
#include <linux/semaphore.h>
#include <linux/wait.h>

#ifndef MYMDEV_H_
#define MYMDEV_H_

//预设主设备号
#ifndef MYMDEV_MAJOR
#define MYMDEV_MAJOR 500
#endif

//定义次设备个数
#ifndef MYMDEV_MINOR_COUNT
#define MYMDEV_MINOR_COUNT 1
#endif

//定义模拟设备内容大小
#ifndef MYMDEV_MEM_SIZE
#define MYMDEV_MEM_SIZE 80
#endif

//定义模拟设备描述结构
struct mymdev{
	char* data;
	unsigned long size;
	struct semaphore sema;
	struct semaphore sema_write;
	wait_queue_head_t read_queue,write_queue;
};

#ifndef MYMDEV_IOCTL__
#define MYMDEV_IOCTL__

#define MYMDEV_MAGIC_ 'z'
#define MYMDEV_UN_PARAM_CMD _IO(MYMDEV_MAGIC_,0)
#define MYMDEV_PARAM_SIZE int
#define MYMDEV_R_CMD _IOR(MYMDEV_MAGIC_,1,MYMDEV_PARAM_SIZE)
#define MYMDEV_W_CMD _IOW(MYMDEV_MAGIC_,2,MYMDEV_PARAM_SIZE)
#define MYMDEV_W_CHAR_CMD _IOW(MYMDEV_MAGIC_,3,char)
#define MYMDEV_CMD_SIZE 8
#endif

#endif /* MYMDEV_H_ */
