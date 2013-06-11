/*
 * hci-test.c
 *
 *  Created on: 2012-9-26
 *  Author    : zekezang
 *
 *	build cmd :


arm-linux-gcc hci-test.c -o hci-test \
-I/sourcecode/arm-workspace/bluetooth-test/include \
-L/sourcecode/arm-workspace/bluetooth-test/lib \
-lbluetooth -O2


 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <bluetooth/bluetooth.h> 	//BTPROTO_HCI
#include <bluetooth/hci.h>          //struct hci_dev_info
#include <bluetooth/hci_lib.h>     	//hci_devid()
#include <bluetooth/l2cap.h>      	//l2cap
#include <bluetooth/hidp.h>       	//hidp


struct hci_dev_list_req *dlr;
struct hci_dev_req *dr;

struct hci_dev_info di;

int ctl = 0;

int main(int argc, char **argv) {

	if (!(dlr = malloc(HCI_MAX_DEV * sizeof(struct hci_dev_req) + sizeof(uint16_t)))) {
		perror("Can't allocate memory");
	}

	dlr->dev_num = HCI_MAX_DEV;
	dr = dlr->dev_req;

	ctl = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
	if (ctl < 0) {
		perror("Can't open HCI socket.");
	}

	if (ioctl(ctl, HCIGETDEVLIST, (void *)dlr) < 0) {
		perror("Can't get device list");
	}

	int i = 0;
	for(i=0; i<dlr->dev_num; i++){
		printf("---dev_id:%d---\n", dr[i].dev_id);
	}

	di.dev_id = dr[i].dev_id;
	if(ioctl(ctl, HCIGETDEVINFO, &di) < 0){
		perror("Can't get device info");
	}

	printf("---dev_name:%s---\n",di.name);


	dr[0].dev_opt = SCAN_PAGE | SCAN_INQUIRY;
	if(ioctl(ctl, HCISETSCAN, (unsigned long) dr) < 0){
		perror("Can't HCISETSCAN");
	}


//	dr[0].dev_opt = SCAN_DISABLED;
//	if(ioctl(ctl, HCISETSCAN, (unsigned long) dr) < 0){
//		perror("Can't HCISETSCAN");
//	}



//	////search
//
//	struct hci_inquiry_req *ir;
//	void* buf = malloc(sizeof(*ir) + (sizeof(inquiry_info) * 200));
//	if (!buf){
//		perror("Can't malloc");
//		goto done;
//	}
//
//	ir = buf;
//	ir->dev_id  = di.dev_id;
//	ir->num_rsp = 0;
//	ir->length  = 8;
//	ir->flags   = 0;
//
//	//0x33, 0x8b, 0x9e
//	ir->lap[0] = 0x33;
//	ir->lap[1] = 0x8b;
//	ir->lap[2] = 0x9e;
//
//
//	if(ioctl(ctl, HCIINQUIRY, (unsigned long) buf) < 0){
//		perror("Can't HCIINQUIRY----");
//		goto done;
//	}
//	printf("---ir->num_rsp:%d---\n",ir->num_rsp);
//
//	int ssize = sizeof(inquiry_info)*ir->num_rsp;
//	inquiry_info *ii = malloc(ssize);
//	memcpy(ii,buf+sizeof(*ir),ssize);
//
//	int k = 0;
//	for(k = 0; k<ir->num_rsp; k++){
//		char addr[64];
//		ba2str(&(ii[k].bdaddr), addr);
//		printf("---ir->ba2str:%s--dev_class:[0]=%d-----[1]=%d----[2]=%d---\n",addr, ii[k].dev_class[0], ii[k].dev_class[1], ii[k].dev_class[2]);
//	}



	//////connection







done:
	return 0;
}

