#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv) {

	char bs[2048];
	int fd;
//	fd = open("/dev/sdc", O_RDWR);
//	if (fd < 0) {
//		printf("%s\n", strerror(errno));
//		return;
//	}
//
//	memset(bs,0,2048);
//
//	strcpy(bs,"zekezang");
//
//	lseek(fd,1024*1024*10,SEEK_CUR);
//
//	int l = write(fd,bs,8);
//
//	printf("---w= %d\n", l);
//
//	close(fd);
//

	sleep(1);
//
//
	fd = open("/dev/sdc", O_RDWR);
	if (fd < 0) {
		printf("%s\n", strerror(errno));
		return;
	}

	lseek(fd,1024*1024*10,SEEK_CUR);
	int k = read(fd,bs,8);

	printf("---r= %d\n", k);

	int i = 0;
	for(i =0 ;i<8; i++){
		printf("---bs[%d]=%X\n", i,bs[i]);
	}

	close(fd);
	return 0;
}


