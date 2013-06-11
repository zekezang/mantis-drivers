#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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
	struct sockaddr_rc addr = {
			0 };
	struct sockaddr_rc local;
	int s, status;
	char *buf;
	char dest[32];
	strcpy(dest, "3F:E6:3D:B3:E7:44");

	printf("client--->Creating socket...\n");

	s = socket(PF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
	if (s < 0) {
		perror("client---->create socket error");
	}
	// set the connection parameters (who to connect to )
	addr.rc_family = AF_BLUETOOTH;
	addr.rc_channel = (uint8_t) 1;
	str2ba(dest, &addr.rc_bdaddr);

	//bind
	local.rc_family = AF_BLUETOOTH;
	str2ba("04:E0:24:6B:FB:E9", &local.rc_bdaddr);
	int result = bind(s, (struct sockaddr *) &local, sizeof(local));
	if (result < 0) {
		perror("client---->bind socket error");
	}

	// connect to server
	printf("client--->connectting...\n");
	status = connect(s, (struct sockaddr *) &addr, sizeof(addr));
	// send a message
	if (status == 0) {
		printf("client--->scuess!\n");

		int kk = 10;
		while (kk > 0) {
			status = write(s, "hello!", 6);
			printf("client--->status:%d--!\n", status);
			kk--;
		}

		do {
			scanf("%s", buf);
			status = write(s, buf, strlen(buf));
			if (status < 0)
				perror("client---->oooo  no!!!\n");
		} while (strcmp(buf, "1") != 0);

	} else {
		perror("client--->Failed!");
	}
	close(s);
	return 0;
}
