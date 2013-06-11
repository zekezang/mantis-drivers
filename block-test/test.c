#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv) {

	char bs[80];
	int fd;
	fd = open("/dev/test001", O_RDWR);
	if (fd < 0) {
		printf("%s/n", strerror(errno));
		return;
	}


//	read(fd,bs,0);

	printf("-------fork------\n");
	pid_t fpid = fork();
	if (fpid == 0) {
//		ioctl(fd, 0);
		write(fd,"ss",0);
	} else {
//		ioctl(fd, 0);
		write(fd,"ss",0);
	}
	close(fd);
	return 0;
}
