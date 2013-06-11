#include"uart.h"

#define DEBUG
#define	DEV		"/dev/ttyzekezang0"

int speed_arr[] = {
		B38400,
		B115200,
		B19200,
		B9600,
		B4800,
		B2400,
		B1200,
		B300 };
int name_arr[] = {
		38400,
		115200,
		19200,
		9600,
		4800,
		2400,
		1200,
		300 };

int set_parity(int fd, int databits, int stopbits, int parity) {
#ifdef	DEBUG
	printf("fd = %d\tdatabits = %d\tstopbits = %d\tparity = %c\n", fd, databits, stopbits, parity);
#endif

	struct termios options;

//	if (tcgetattr(fd, &options) != 0) {
//		perror("SetupSerial 1");
//		return (FALSE);
//	}
//	options.c_cflag &= ~CSIZE;

	switch (databits) {
	case 7:
		options.c_cflag |= CS7;
		break;
	case 8:
		options.c_cflag |= CS8;
		break;
	default:
		fprintf(stderr, "Unsupported data size\n");
		return (FALSE);
	}
	switch (parity) {
	case 'n':
	case 'N':
		options.c_cflag &= ~PARENB; /* Clear parity enable */
		options.c_iflag &= ~INPCK; /* Enable parity checking */
		break;
	case 'o':
	case 'O':
		options.c_cflag |= (PARODD | PARENB); /* 设置为奇效验*/
		options.c_iflag |= INPCK; /* Disnable parity checking */
		break;
	case 'e':
	case 'E':
		options.c_cflag |= PARENB; /* Enable parity */
		options.c_cflag &= ~PARODD; /* 转换为偶效验*/
		options.c_iflag |= INPCK; /* Disnable parity checking */
		break;
	case 'S':
	case 's': /*as no parity*/
		options.c_cflag &= ~PARENB;
		options.c_cflag &= ~CSTOPB;
		break;
	default:
		fprintf(stderr, "Unsupported parity\n");
		return (FALSE);
	}
	/* 设置停止位*/
	switch (stopbits) {
	case 1:
		options.c_cflag &= ~CSTOPB;
		break;
	case 2:
		options.c_cflag |= CSTOPB;
		break;
	default:
		fprintf(stderr, "Unsupported stop bits\n");
		return (FALSE);
	}
#if 0
	/* Set input parity option */
	if (parity != 'n')
		options.c_iflag |= INPCK;
	tcflush(fd, TCIFLUSH);
	options.c_cc[VTIME] = 150; /* 设置超时15 seconds*/
	options.c_cc[VMIN] = 0; /* Update the options and do it NOW */
	if (tcsetattr(fd, TCSANOW, &options) != 0) {
		perror("SetupSerial 3");
		return (FALSE);
	}
#endif
	return (TRUE);
}

void set_speed(int fd, int speed) {
#ifdef DEBUG
	printf("fd = %d, speed = %d\n", fd, speed);
#endif

	int i;
	int status;
	struct termios Opt;

	tcgetattr(fd, &Opt);
	for (i = 0; i < sizeof(speed_arr) / sizeof(int); i++) {
		if (speed == name_arr[i]) {
			tcflush(fd, TCIOFLUSH);
			cfsetispeed(&Opt, speed_arr[i]);
			cfsetospeed(&Opt, speed_arr[i]);
			status = tcsetattr(fd, TCSANOW, &Opt);
			if (status != 0) {
				perror("tcsetattr fd");
				return;
			}
			tcflush(fd, TCIOFLUSH);
		}
	}
}
int open_dev(void) {
//	int fd = open(DEV, O_RDWR | O_NONBLOCK | O_NOCTTY);
	int fd = open(DEV, O_RDWR);
#ifdef DEBUG
	printf("Open = %s\n", DEV);
#endif

	if (-1 == fd) {
		perror("Can't Open Serial Port");
		return -1;
	} else
		return fd;
}

void get_time(char *buf) {
	char *wday[] = {
			"Sun",
			"Mon",
			"Tue",
			"Wed",
			"Thu",
			"Fri",
			"Sat" };
	time_t timep;
	struct tm *p;

	time(&timep);
	p = localtime(&timep);
	sprintf(buf, "%d-%d-%d %s %d:%d:%d\n", (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday, wday[p->tm_wday], p->tm_hour, p->tm_min, p->tm_sec);

	return;
}

int main(int argc, char *argv[]) {
	int fd;
	int nwrite, nread;
	char r_buf[512], w_buf[512];

	fd = open_dev();

//	set_speed(fd, 115200);
//	if (set_parity(fd, 8, 1, 'N') == FALSE) {
//		printf("Set Parity Error ");
//		exit(0);
//	}
//
//	while (1) {
//		memset(w_buf, 0, sizeof(w_buf));
//		get_time(w_buf);
//
//		nwrite = write(fd, w_buf, 30);
//		printf("w_buf = %s\n", w_buf);
//		sleep(1);
//
//#if 0
//		memset(r_buf, 0, sizeof(r_buf));
//		while((nread = read(fd, r_buf, 30)) > 0)
//		{
//			printf("r_buf = %s\n", r_buf);
//			memset(r_buf, 0, sizeof(r_buf));
//		}
//#endif
//	}

	return 0;
}
