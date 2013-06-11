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
	ioctl(fd, 0);

	close(fd);
	return 0;
}
