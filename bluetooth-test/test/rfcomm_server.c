#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>


void baswap(bdaddr_t *dst, const bdaddr_t *src)
{
	register unsigned char *d = (unsigned char *) dst;
	register const unsigned char *s = (const unsigned char *) src;
	register int i;

	for (i = 0; i < 6; i++)
		d[i] = s[5-i];
}

int ba2str(const bdaddr_t *ba, char *str)
{
	uint8_t b[6];

	baswap((bdaddr_t *) b, ba);
	return sprintf(str, "%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X",
		b[0], b[1], b[2], b[3], b[4], b[5]);
}

int str2ba(const char *str, bdaddr_t *ba)
{
	uint8_t b[6];
	const char *ptr = str;
	int i;

	for (i = 0; i < 6; i++) {
		b[i] = (uint8_t) strtol(ptr, NULL, 16);
		if (i != 5 && !(ptr = strchr(ptr, ':')))
			ptr = ":00:00:00:00:00";
		ptr++;
	}

	baswap(ba, (bdaddr_t *) b);

	return 0;
}


int main(int argc, char **argv) {
	struct sockaddr_rc loc_addr = {
			0 }, rem_addr = {
			0 };
	char buf[1024] = {
			0 }; //,*addr;

	int s, client, bytes_read, result;
	int opt = sizeof(rem_addr);

	long fset = 0;

	printf("server--->Creating socket...\n");

	s = socket(PF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
	if (s < 0) {
		perror("server----->create socket error");
		exit(1);
	} else {
		printf("server--->success!\n");
	}
	loc_addr.rc_family = AF_BLUETOOTH;

	str2ba("3F:E6:3D:B3:E7:44", &loc_addr.rc_bdaddr);
//	str2ba("00:15:83:3D:0A:11", &loc_addr.rc_bdaddr);
	loc_addr.rc_channel = (uint8_t) 1;

	printf("server--->Binding socket...\n");
	result = bind(s, (struct sockaddr *) &loc_addr, sizeof(loc_addr));
	if (result < 0) {
		perror("server----->bind socket error:");
		exit(1);
	} else {
		printf("server--->success!\n");
	}

	//result=ba2str(&loc_addr.rc_bdaddr,addr);

	printf("server--->Listen... ");
	result = listen(s, 1);
	if (result < 0) {
		perror("server----->listen error:");
		exit(1);
	} else {
		printf("server--->requested!\n");
	}
	printf("server--->Accepting...\n");
	client = accept(s, (struct sockaddr *) &rem_addr, &opt);
	if (client < 0) {
		perror("server----->accept error");
		exit(1);
	} else {

		//fset = fcntl(client, F_GETFL, 0);
		//printf("server--->111111fset & O_NONBLOCK = [%d]\n", fset & O_NONBLOCK);

		//fcntl(client, F_SETFL, fset | O_NONBLOCK);

		//fset = fcntl(client, F_GETFL, 0);
		//printf("server--->2222222fset & O_NONBLOCK = [%d]\n", fset & O_NONBLOCK);

		//printf("server--->set O_NONBLOCK ok!\n");
	}
	ba2str(&rem_addr.rc_bdaddr, buf);
	printf("server--->accepted connection from %s \n", buf);
	memset(buf, 0, sizeof(buf));

	int kk = 300;
	while (kk > 0) {
		usleep(1);
		printf("server--->kk[%d]\n", kk);
		printf("server--->333333fset & O_NONBLOCK = [%d]\n", fset & O_NONBLOCK);
		bytes_read = read(client, buf, sizeof(buf));
		if (bytes_read > 0) {
			printf("server--->received[%s]\n", buf);
//			if (strcmp(buf, "1") == 0)
//				exit(1);
			memset(buf, 0, bytes_read);
		}
		kk--;
	}

	close(client);
	close(s);
	return 0;
}
