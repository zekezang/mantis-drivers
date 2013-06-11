/*
 * memdev.h
 *
 *  Created on: 2012-4-9
 *      Author: zekezang
 */

#include <linux/semaphore.h>

#ifndef MEMDEV_H_
#define MEMDEV_H_

#define mem_size 1024


//定义模拟设备描述结构
struct memdev_struct{
	char* data;
	unsigned long size;
	struct semaphore sema;
	struct semaphore sema_write;
	wait_queue_head_t read_queue,write_queue;
};

struct memdev{
	struct device memdev_dev;
	struct memdev_struct memdev_struct;
};

static struct class *myclass;

extern struct memdev* memdev;

extern struct bus_type memdev_bus_type;




#endif /* MEMDEV_H_ */
