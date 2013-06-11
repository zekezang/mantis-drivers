/*
 * l2captest.c
 *
 *  Created on: 2012-10-8
 *      Author: zekezang
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <bluetooth/bluetooth.h> 	//BTPROTO_HCI
#include <bluetooth/hci.h>          //struct hci_dev_info
#include <bluetooth/hci_lib.h>     	//hci_devid()
#include <bluetooth/l2cap.h>      	//l2cap
#include <bluetooth/hidp.h>       	//hidp
int main(int argc, char** argv) {
	int l2_sck = 0;
	struct sockaddr_l2 local_l2_addr;
	struct sockaddr_l2 remote_l2_addr;
	int len = 0;
	char* send_buf;
	int id = 1; //不要为0

	send_buf = malloc(L2CAP_CMD_HDR_SIZE);

	// create l2cap raw socket
//	l2_sck = socket(PF_BLUETOOTH, SOCK_RAW, BTPROTO_L2CAP); //创建L2CAP protocol的RAW Packet
//	if (l2_sck < 0) {
//		perror("\nsocket:");
//		return -1;
//	}
//
//	//bind
//	memset(&local_l2_addr, 0, sizeof(struct sockaddr_l2));
//	local_l2_addr.l2_family = PF_BLUETOOTH;
//
//	char s[64] = "00:15:83:3D:0A:00";
//	str2ba(s, &local_l2_addr.l2_bdaddr);
//
//	if (bind(l2_sck, (struct sockaddr*) &local_l2_addr, sizeof(struct sockaddr_l2)) < 0) {
//		perror("\nbind()");
//	}
//	char ss[100];
//	ba2str(&local_l2_addr.l2_bdaddr, ss);
//	printf("\nbind local addr %s\n", ss);
//
//	//connect
//	memset(&remote_l2_addr, 0, sizeof(struct sockaddr_l2));
//	remote_l2_addr.l2_family = PF_BLUETOOTH;
//	printf("\nConnect to 00:15:83:3D:0A:11\n");
//
//	str2ba("00:15:83:3D:0A:11", &remote_l2_addr.l2_bdaddr);
//
//	if (connect(l2_sck, (struct sockaddr*) &remote_l2_addr, sizeof(struct sockaddr_l2)) < 0) {
//		perror("\nconnect()");
//	}
//
//	l2cap_cmd_hdr *send_cmd = (l2cap_cmd_hdr *) send_buf;
//
//	send_cmd->ident = id;
//	send_cmd->len = 0;
//	send_cmd->code = L2CAP_CONN_REQ;
//
//	send_cmd->ident = id;
//	if (send(l2_sck, send_buf, L2CAP_CMD_HDR_SIZE, 0) <= 0) {
//		perror("\nsend():");
//	}
//	close(l2_sck);

	//------------------------------------------------------------------------------------------------------
	l2_sck = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP); //创建L2CAP protocol的RAW Packet
	if (l2_sck < 0) {
		perror("\nsocket:");
		return -1;
	}

	//bind
	memset(&local_l2_addr, 0, sizeof(struct sockaddr_l2));
	local_l2_addr.l2_family = PF_BLUETOOTH;

	strcmp("00:15:83:3D:0A:00", s);
	str2ba(s, &local_l2_addr.l2_bdaddr);

	if (bind(l2_sck, (struct sockaddr*) &local_l2_addr, sizeof(struct sockaddr_l2)) < 0) {
		perror("\nbind()");
	}

//	struct l2cap_options opts;
//	// in mtu 和 out mtu.每个包的最大值
//	memset(&opts, 0, sizeof(opts));
//	optlen = sizeof(opts);
//	getsockopt(sk, SOL_L2CAP, L2CAP_OPTIONS, &opts, &optlen);
//	printf("/nomtu:[%d]. imtu:[%d]. flush_to:[%d]. mode:[%d]/n", opts.omtu, opts.imtu, opts.flush_to, opts.mode);
//
//	//set opts. default value
//	opts.omtu = 0;
//	opts.imtu = 672;
//	if (setsockopt(sk, SOL_L2CAP, L2CAP_OPTIONS, &opts, sizeof(opts)) < 0) {
//		perror("/nsetsockopt():");
//		exit(0);
//	}
//
//	ba2str(&local_l2_addr.l2_bdaddr, ss);
//	printf("\nbind local addr %s\n", ss);

	//connect
	memset(&remote_l2_addr, 0, sizeof(struct sockaddr_l2));
	remote_l2_addr.l2_family = PF_BLUETOOTH;
	printf("\nConnect to 00:15:83:3D:0A:11\n");

	str2ba("00:15:83:3D:0A:11", &remote_l2_addr.l2_bdaddr);

	if (connect(l2_sck, (struct sockaddr*) &remote_l2_addr, sizeof(struct sockaddr_l2)) < 0) {
		perror("\nconnect()");
	}

	char send_char[50];
	int i = 0;
	for (i = 0; i < 50; i++) {
		send_char[i] = 'A';
	}
	if (send(l2_sck, send_char, 50, 0) <= 0) {
		perror("\nsend():");
	}

	close(l2_sck);

	return 0;
}
