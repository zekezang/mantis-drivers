
/**   zekezang  ***/

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include <bluetooth/bluetooth.h> 	//BTPROTO_HCI
#include <bluetooth/hci.h>          //struct hci_dev_info
#include <bluetooth/hci_lib.h>     	//hci_devid()
#include <bluetooth/l2cap.h>      	//l2cap
#include <bluetooth/hidp.h>       	//hidp

void send_thread(void* data) {
//	printf("%s----%lu\n", "l2client", pthread_self());
	int l2_sck = 0;
	struct sockaddr_l2 local_l2_addr;
	struct sockaddr_l2 remote_l2_addr;
	int len = 0;
	char* send_buf;
	int add_buf_size = 512;
	int id = 1; //不要为0

	send_buf = malloc(add_buf_size);

	// create l2cap raw socket
	l2_sck = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP); //创建L2CAP protocol的RAW Packet
	if (l2_sck < 0) {
		perror("\nsocket:");
		_exit(1);
	}

	//bind
	memset(&local_l2_addr, 0, sizeof(struct sockaddr_l2));
	local_l2_addr.l2_family = PF_BLUETOOTH;

	char s[64];
	strcpy(s, data);
	str2ba(s, &local_l2_addr.l2_bdaddr);

	if (bind(l2_sck, (struct sockaddr*) &local_l2_addr, sizeof(struct sockaddr_l2)) < 0) {
		perror("\nbind()");
	}
//	char ss[100];
//	ba2str(&local_l2_addr.l2_bdaddr, ss);
//	printf("\nbind local addr %s\n", ss);

	//connect
	memset(&remote_l2_addr, 0, sizeof(struct sockaddr_l2));
	remote_l2_addr.l2_family = PF_BLUETOOTH;
	remote_l2_addr.l2_psm = htobs(0x1001);
	str2ba("00:15:83:3D:0A:22", &remote_l2_addr.l2_bdaddr);

	if (connect(l2_sck, (struct sockaddr*) &remote_l2_addr, sizeof(struct sockaddr_l2)) < 0) {
		perror("\nconnect()");
	}

//	l2cap_cmd_hdr *send_cmd = (l2cap_cmd_hdr *) send_buf;
//	send_cmd->ident = id;
//	send_cmd->len = htobs(add_buf_size);
//	send_cmd->code = L2CAP_CONN_REQ;

	int kk=0;
	for(kk=0; kk<1000; kk++){
		sprintf(send_buf, "%s %d", "AAAAAAAAAABBBBBBBBBBCCCCCCCCCCDDDDDDDDDDEEEEEEEEEEFFFFFFFFFFABCD",kk);
		printf("send %d : %s\n",kk,send_buf);
		if (send(l2_sck, send_buf, strlen(send_buf), 0) <= 0) {
			perror("\nsend():");
		}
//		sleep(5);
	}

	sprintf(send_buf, "%s", "end");
	if (send(l2_sck, send_buf, strlen(send_buf), 0) <= 0) {
		perror("\nsend():");
	}

	close(l2_sck);

//	pthread_detach(pthread_self());
}

void server(void *data) {
	struct sockaddr_l2 loc_addr = { 0 }, rem_addr = { 0 };
	char buf[512] = { 0 };
	int s, client, bytes_read;
	socklen_t opt = sizeof(rem_addr);

	// allocate socket
	s = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);

	// bind socket to port 0x1001 of the first available
	// bluetooth adapter
	loc_addr.l2_family = AF_BLUETOOTH;
	str2ba("00:15:83:3D:0A:22", &loc_addr.l2_bdaddr);
	loc_addr.l2_psm = htobs(0x1001);

	if (bind(s, (struct sockaddr *) &loc_addr, sizeof(loc_addr)) < 0) {
		perror("\nserver -bind()");
	}

	struct l2cap_options opts;
	// in mtu 和 out mtu.每个包的最大值
	memset(&opts, 0, sizeof(opts));
	int optlen = sizeof(opts);
	getsockopt(s, SOL_L2CAP, L2CAP_OPTIONS, &opts, &optlen);
	printf("omtu:[%d]. imtu:[%d]. flush_to:[%d]. mode:[%d]\n", opts.omtu, opts.imtu, opts.flush_to, opts.mode);

	//set opts. default value
	opts.omtu = 0;
	opts.imtu = 65535;
	if (setsockopt(s, SOL_L2CAP, L2CAP_OPTIONS, &opts, sizeof(opts)) < 0) {
		perror("/nsetsockopt():");
		_exit(0);
	}
	getsockopt(s, SOL_L2CAP, L2CAP_OPTIONS, &opts, &optlen);
	printf("omtu:[%d]. imtu:[%d]. flush_to:[%d]. mode:[%d]\n", opts.omtu, opts.imtu, opts.flush_to, opts.mode);

	// put socket into listening mode
	if (listen(s, 5) < 0) {
		perror("\nserver -listen()");
	}

	// accept one connection
	client = accept(s, (struct sockaddr *) &rem_addr, &opt);
	char buff[100];
	ba2str(&rem_addr.l2_bdaddr, buff);
	while(1){
		memset(buf, 0, sizeof(buf));
		// read data from the client
		bytes_read = recv(client, buf, sizeof(buf),0);
		if (bytes_read > 0) {
			printf("receve from %s : [%s]\n", buff, buf);
			if(strcmp(buf,"end")==0){
				break;
			}
		}
	}

	// close connection
	close(client);
	close(s);
}

char bdadd_1[64] = "00:15:83:3D:0A:11";
char bdadd_2[64] = "00:15:83:3D:0A:22";

int main(int argc, char** argv) {

	pthread_t l2client_1, l2client_2;

	if (pthread_create(&l2client_2, NULL, (void*) server, bdadd_2) < 0) {
		perror("\nl2client:");
		_exit(1);
	}

	sleep(1);

	if (pthread_create(&l2client_1, NULL, (void*) send_thread, bdadd_1) < 0) {
		perror("\nl2client:");
		_exit(1);
	}
//
//
//
	pthread_join(l2client_1, NULL);
	pthread_join(l2client_2, NULL);

//	sleep(100000);
	return 0;
}
