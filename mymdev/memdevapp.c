#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

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

int main(int argc, char **argv){
	int fd;
	char b[80];
	char* s = "zekezang12345";

//		printf("buf is %s\n", buf);
	//

	if(fork()){
		if((fd = open("/dev/mymdev", O_RDWR)) == -1)
			printf("open memdev WRONG\n");
		lseek(fd, 0, SEEK_SET);
		read(fd, b, 80);
		close(fd);
		printf("read:%s\n", b);

	}else{
		if((fd = open("/dev/mymdev", O_RDWR)) == -1)
			printf("open memdev WRONG\n");
		lseek(fd, 0, SEEK_SET);

		usleep(3000000);
		size_t i = write(fd, s, strlen(s));

//		printf("%d\n", i);
		close(fd);
	}

	//
	//	lseek(fd, 0, SEEK_SET);
	//
	//	read(fd, buf_read, sizeof(buf));
	//
	//	printf("buf_read is %s\n", buf_read);

	//	int i = ioctl(fd, MYMDEV_UN_PARAM_CMD, NULL);

	//	int a = 6;
	//	int i = ioctl(fd, MYMDEV_W_CMD, &a);
	//
	//	char s ='z';
	//	ioctl(fd, MYMDEV_W_CHAR_CMD, &s);
	//
	//
	//	ioctl(fd, MYMDEV_R_CMD, &a);
	//
	//	printf("----%d\n", a);

	return 0;
}
