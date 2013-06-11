/*
 * main.c
 *
 *  Created on: 2013-4-25
 *      Author: zekezang
 */

#include <stdio.h>
#include <fcntl.h>
#include <linux/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	int i = 0;
	unsigned int value[1024];
	int fd;
	fd = open("/sys/devices/platform/s3c2440-i2c/i2c-0/0-0050/eeprom", O_RDWR ); // | O_APPEND
	if (fd < 0) printf("error\n");

	lseek(fd,3,SEEK_CUR);
	int k = write(fd,"zekezang",8);
	printf("----k:%d\n",k);
//
//	close(fd);
//
//	sleep(1);
//
//	fd = open("/sys/devices/platform/s3c2440-i2c/i2c-0/0-0050/eeprom", O_RDWR ); // | O_APPEND
//
//
////	lseek(fd,1,SEEK_CUR);
//	read(fd, value, 1024);
//	printf("read:%s\n", value);

	close(fd);
	return 0;
}
