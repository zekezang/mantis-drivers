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
	int iRel = 0;
	struct sockaddr_l2 local_l2_addr;
	struct sockaddr_l2 remote_l2_addr;
	char str[24] = { 0 };
	int len = 0;
	int size = 50;
	char* send_buf;
	char* recv_buf;
	int i = 0;
	int id = 1; //不要为0

	send_buf = malloc(L2CAP_CMD_HDR_SIZE + size);
	recv_buf = malloc(L2CAP_CMD_HDR_SIZE + size);

	if (argc < 2) {
		printf("\n%s <bdaddr>\n", argv[0]);

	}

	// create l2cap raw socket
	l2_sck = socket(PF_BLUETOOTH, SOCK_RAW, BTPROTO_L2CAP); //创建L2CAP protocol的RAW Packet

	if (l2_sck < 0) {
		perror("\nsocket:");
		return -1;
	}

	//bind
	memset(&local_l2_addr, 0, sizeof(struct sockaddr_l2));
	local_l2_addr.l2_family = PF_BLUETOOTH;

	char s[64] = "00:15:83:3D:0A:11";
	str2ba(s, &local_l2_addr.l2_bdaddr);

//	bacpy(&local_l2_addr.l2_bdaddr, );

	iRel = bind(l2_sck, (struct sockaddr*) &local_l2_addr, sizeof(struct sockaddr_l2));
	if (iRel < 0) {
		perror("\nbind()");
	}
	char ss[100];
	ba2str(&local_l2_addr.l2_bdaddr,ss);
	printf("\nbind local addr %s\n", ss);


	//connect
	memset(&remote_l2_addr, 0, sizeof(struct sockaddr_l2));
	remote_l2_addr.l2_family = PF_BLUETOOTH;
	printf("\nConnect to %s\n", argv[1]);
	str2ba(argv[1], &remote_l2_addr.l2_bdaddr);

	iRel = connect(l2_sck, (struct sockaddr*) &remote_l2_addr, sizeof(struct sockaddr_l2));
	if (iRel < 0) {
		perror("\nconnect()");

	}

	//get local bdaddr
	len = sizeof(struct sockaddr_l2);
	memset(&local_l2_addr, 0, sizeof(struct sockaddr_l2));

//注意，getsockname（）参数三是一个输入输出参数。输入时，为参数二的总体长度。输出时，

//为实际长度。
	iRel = getsockname(l2_sck, (struct sockaddr*) &local_l2_addr, &len);
	if (iRel < 0) {
		perror("\ngetsockname()");
	}
	ba2str(&(local_l2_addr.l2_bdaddr), str);
	//printf("\nLocal Socket bdaddr:[%s]\n", str);
	printf("l2ping: [%s] from [%s](data size %d) ...\n", argv[1], str, size);

	for (i = 0; i < size; i++){
		send_buf[L2CAP_CMD_HDR_SIZE + i] = 'z';
	}
	printf("------------------i:%d\n",i);

	l2cap_cmd_hdr *send_cmd = (l2cap_cmd_hdr *) send_buf;
	l2cap_cmd_hdr *recv_cmd = (l2cap_cmd_hdr *) recv_buf;

	send_cmd->ident = id; //如上图所示，这一项为此Command Identifier
	send_cmd->len = htobs(size);
	send_cmd->code = L2CAP_ECHO_REQ; //如上图所示，此项为Command code.这项定为：/Echo Request。对端会发送Response回来。code=L2CAP_ECHO_RSP

	while (1) {
		send_cmd->ident = id;
		if (send(l2_sck, send_buf, size + L2CAP_CMD_HDR_SIZE, 0) <= 0) {
			perror("\nsend():");
		}

		while (1) {
			if (recv(l2_sck, recv_buf, size + L2CAP_CMD_HDR_SIZE, 0) <= 0) {
				perror("\nrecv()");
			}

			if (recv_cmd->ident != id)
				continue;

			if (recv_cmd->code == L2CAP_ECHO_RSP) {
				//printf("\nReceive Response Packet.\n");
				printf("%d bytes from [%s] id %d----%s\n", recv_cmd->len, argv[1], recv_cmd->ident,recv_buf+L2CAP_CMD_HDR_SIZE);
				break;
			}

		}
		sleep(1);
		id++;

	}

	close(l2_sck);

	return 0;
}
