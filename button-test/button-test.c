/*
 * button-test.c
 *
 *  Created on: 2012-5-16
 *      Author: zekezang
 */

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

int main() {
	int button;
	int ret, i;
	int c;
	fd_set readfd;
	struct timeval timeout;
	button = open("/dev/my-button1", O_RDONLY | O_NONBLOCK);
	printf("button is %d\n", button);
	if (button >= 0) {
		while (1) {
			timeout.tv_sec = 1;
			timeout.tv_usec = 0;
			FD_ZERO(&readfd);
			FD_SET(button, &readfd);
			ret = select(button + 1, &readfd, NULL, NULL, &timeout);
			if (FD_ISSET(button,&readfd)) {
				i = read(button, &c, sizeof(int));
				printf("hehethe input is %c\n", c);
			}
		}
	}

}
