/*
 *		UDISK Driver For SUMSUNG Mobile
 *		BY CSK
 *		
 *		SPECIAL THX TO Michael Gee (michael@linuxspecific.com)
 */


//Something to be include...
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/freezer.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/usb.h>
#include <linux/usb_usual.h>
#include <linux/blkdev.h>
#include <linux/smp_lock.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include <linux/highmem.h>
//SCSI support
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_devinfo.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_eh.h>

#include <asm/scatterlist.h>
#include <asm/dma.h> // I hate this guy...


struct us_data;
struct scsi_cmnd;



/* command block wrapper */
struct bulk_cb_wrap {
	__le32	Signature;		/* contains 'USBC' */
	__u32	Tag;			/* unique per command id */
	__le32	DataTransferLength;	/* size of data */
	__u8	Flags;			/* direction in bit 0 */
	__u8	Lun;			/* LUN normally 0 */
	__u8	Length;			/* of of the CDB */
	__u8	CDB[16];		/* max command */
};

#define US_BULK_CB_WRAP_LEN	31
#define US_BULK_CB_SIGN		0x43425355	/*spells out USBC */
#define US_BULK_FLAG_IN		1
#define US_BULK_FLAG_OUT	0

/* command status wrapper */
struct bulk_cs_wrap {
	__le32	Signature;		/* should = 'USBS' */
	__u32	Tag;			/* same as original command */
	__le32	Residue;		/* amount not transferred */
	__u8	Status;			/* see below */
	__u8	Filler[18];
};

#define US_BULK_CS_WRAP_LEN	13
#define US_BULK_CS_SIGN		0x53425355	/* spells out 'USBS' */
#define US_BULK_STAT_OK		0
#define US_BULK_STAT_FAIL	1
#define US_BULK_STAT_PHASE	2

/* bulk-only class specific requests */
#define US_BULK_RESET_REQUEST	0xff
#define US_BULK_GET_MAX_LUN	0xfe



#define USB_STOR_XFER_GOOD	0	/* good transfer                 */
#define USB_STOR_XFER_SHORT	1	/* transferred less than expected */
#define USB_STOR_XFER_STALLED	2	/* endpoint stalled              */
#define USB_STOR_XFER_LONG	3	/* device tried to send too much */
#define USB_STOR_XFER_ERROR	4	/* transfer died in the middle   */
/*
 * Transport return codes
 */

#define USB_STOR_TRANSPORT_GOOD	   0   /* Transport good, command good	   */
#define USB_STOR_TRANSPORT_FAILED  1   /* Transport good, command failed   */
#define USB_STOR_TRANSPORT_NO_SENSE 2  /* Command failed, no auto-sense    */
#define USB_STOR_TRANSPORT_ERROR   3   /* Transport bad (i.e. device dead) */


/* Dynamic flag definitions: used in set_bit() etc. */
#define US_FLIDX_URB_ACTIVE	18  /* 0x00040000  current_urb is in use  */
#define US_FLIDX_SG_ACTIVE	19  /* 0x00080000  current_sg is in use   */
#define US_FLIDX_ABORTING	20  /* 0x00100000  abort is in progress   */
#define US_FLIDX_DISCONNECTING	21  /* 0x00200000  disconnect in progress */
#define ABORTING_OR_DISCONNECTING	((1UL << US_FLIDX_ABORTING) | \
					 (1UL << US_FLIDX_DISCONNECTING))
#define US_FLIDX_RESETTING	22  /* 0x00400000  device reset in progress */
#define US_FLIDX_TIMED_OUT	23  /* 0x00800000  SCSI midlayer timed out  */


#define USB_STOR_STRING_LEN 32


#define US_IOBUF_SIZE		64	/* Size of the DMA-mapped I/O buffer */
#define US_SENSE_SIZE		18	/* Size of the autosense data buffer */

#define US_SUSPEND	0
#define US_RESUME	1

/* we use this macro to help us write into the buffer */
#undef SPRINTF
#define SPRINTF(args...) \
	do { if (pos < buffer+length) pos += sprintf(pos, ## args); } while (0)

/* we allocate one of these for every device that we remember */
struct us_data {

	struct mutex		dev_mutex;	 /* protect pusb_dev */
	struct usb_device	*pusb_dev;	 /* this usb_device */
	struct usb_interface	*pusb_intf;	 /* this interface */
	unsigned long		flags;		 /* from filter initially */
	unsigned int		send_bulk_pipe;	 /* cached pipe values */
	unsigned int		recv_bulk_pipe;
	unsigned int		send_ctrl_pipe;
	unsigned int		recv_ctrl_pipe;
	unsigned int		recv_intr_pipe;

	/* informatioscsi_report_bus_reset(us_to_host(us), 0);n about the device */
	__le32			bcs_signature;
	u8			subclass;
	u8			protocol;
	u8			max_lun;

	u8			ifnum;		 /* interface number   */
	u8			ep_bInterval;	 /* interrupt interval */ 

	

	/* SCSI interfaces */
	struct scsi_cmnd	*srb;		 /* current srb		*/
	unsigned int		tag;		 /* current dCBWTag	*/

	/* control and bulk communications data */
	struct urb		*current_urb;	 /* USB requests	 */
	struct usb_ctrlrequest	*cr;		 /* control requests	 */
	struct usb_sg_request	current_sg;	 /* scatter-gather req.  */
	unsigned char		*iobuf;		 /* I/O buffer		 */
	unsigned char		*sensebuf;	 /* sense data buffer	 */
	dma_addr_t		cr_dma;		 /* buffer DMA addresses */
	dma_addr_t		iobuf_dma;

	/* mutual exclusion and synchronization structures */
	struct semaphore	sema;		 /* to sleep thread on	    */
	struct completion	notify;		 /* thread begin/end	    */
	wait_queue_head_t	delay_wait;	 /* wait during scan, reset */



};

static inline struct Scsi_Host *us_to_host(struct us_data *us) {
	return container_of((void *) us, struct Scsi_Host, hostdata);
}
static inline struct us_data *host_to_us(struct Scsi_Host *host) {
	return (struct us_data *) host->hostdata;
}



#define scsi_unlock(host)	spin_unlock_irq(host->host_lock)
#define scsi_lock(host)		spin_lock_irq(host->host_lock)

MODULE_AUTHOR("CSK");
MODULE_DESCRIPTION("SAMSUNG MOBILE USB Mass Storage");
MODULE_LICENSE("GPL");

static atomic_t g_total_workers = ATOMIC_INIT(0);



//SAMSUNG MOBILE USB INFO
#define USB_MASS_VENDOR_ID	0x04e8
#define USB_MASS_PRODUCT_ID	0x665c	

//REG IT
static struct usb_device_id storage_usb_ids [] = {
	{ USB_DEVICE(USB_MASS_VENDOR_ID, USB_MASS_PRODUCT_ID) },	
	{ }					/* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, storage_usb_ids);

unsigned char usb_stor_sense_invalidCDB[18] = {
	[0]	= 0x70,			    /* current error */
	[2]	= ILLEGAL_REQUEST,	    /* Illegal Request = 0x05 */
	[7]	= 0x0a,			    /* additional length */
	[12]	= 0x24			    /* Invalid Field in CDB */
};






//PROTOTYPE
static int usb_worker(void * __us);
static void FreeUpAll(struct us_data *us);
static void quiesce_and_remove_host(struct us_data *us);
static void usb_stor_stop_transport(struct us_data *us);
static int usb_stor_clear_halt(struct us_data *us, unsigned int pipe);
static void usb_stor_invoke_transport(struct scsi_cmnd *srb, struct us_data *us);
static int usb_stor_Bulk_transport(struct scsi_cmnd *srb, struct us_data *us);
static int usb_stor_port_reset(struct us_data *us);
static void usb_stor_report_device_reset(struct us_data *us);
static int usb_stor_Bulk_reset(struct us_data *us);
static int usb_stor_bulk_transfer_buf(struct us_data *us, unsigned int pipe,
	void *buf, unsigned int length, unsigned int *act_len);
static int usb_stor_bulk_transfer_sg(struct us_data* us, unsigned int pipe,
		void *buf, unsigned int length_left, int use_sg, int *residual);
static int usb_stor_control_msg(struct us_data *us, unsigned int pipe,
		 u8 request, u8 requesttype, u16 value, u16 index, 
		 void *data, u16 size, int timeout);
static int usb_stor_msg_common(struct us_data *us, int timeout);
static int interpret_urb_result(struct us_data *us, unsigned int pipe,
		unsigned int length, int result, unsigned int partial);

static int usb_stor_bulk_transfer_sglist(struct us_data *us, unsigned int pipe,
		struct scatterlist *sg, int num_sg, unsigned int length,
		unsigned int *act_len);

static int usb_stor_Bulk_max_lun(struct us_data *us);
static void usb_stor_blocking_completion(struct urb *urb);

static int slave_configure(struct scsi_device *sdev);
static int slave_alloc (struct scsi_device *sdev);
static int bus_reset(struct scsi_cmnd *srb);
static int device_reset(struct scsi_cmnd *srb);
static int command_abort(struct scsi_cmnd *srb);
static int queuecommand(struct scsi_cmnd *srb,
			void (*done)(struct scsi_cmnd *));
static const char* host_info(struct Scsi_Host *host);
static int proc_info (struct Scsi_Host *host, char *buffer,
		char **start, off_t offset, int length, int inout);

static ssize_t store_max_sectors(struct device *dev, struct device_attribute *attr, const char *buf,
		size_t count);

static ssize_t show_max_sectors(struct device *dev, struct device_attribute *attr, char *buf);



static DEVICE_ATTR(max_sectors, S_IRUGO | S_IWUSR, show_max_sectors,
		store_max_sectors);



static struct device_attribute *sysfs_device_attr_list[] = {
		&dev_attr_max_sectors,
		NULL,
		};

struct scsi_host_template usb_stor_host_template = {
	/* basic userland interface stuff */
	.name =				"mobileusb-storage",
	.proc_name =			"mobileusb-storage",
	.proc_info =			proc_info,
	.info =				host_info,

	/* command interface -- queued only */
	.queuecommand =			queuecommand,

	/* error and abort handlers */
	.eh_abort_handler =		command_abort,
	.eh_device_reset_handler =	device_reset,
	.eh_bus_reset_handler =		bus_reset,

	/* queue commands only, only one command per LUN */
	.can_queue =			1,
	.cmd_per_lun =			1,

	/* unknown initiator id */
	.this_id =			-1,

	.slave_alloc =			slave_alloc,
	.slave_configure =		slave_configure,

	/* lots of sg segments can be handled */
	.sg_tablesize =			SG_ALL,

	/* limit the total size of a transfer to 120 KB */
	.max_sectors =                  240,

	/* merge commands... this seems to help performance, but
	 * periodically someone should test to see which setting is more
	 * optimal.
	 */
	.use_clustering =		1,

	/* emulated HBA */
	.emulated =			1,

	/* we do our own delay after a device or bus reset */
	.skip_settle_delay =		1,

	/* sysfs device attributes */
	.sdev_attrs =			sysfs_device_attr_list,

	/* module management */
	.module =			THIS_MODULE
};

///

static void storage_pre_reset(struct usb_interface *iface)
{

	struct us_data *us = usb_get_intfdata(iface);

	/* Make sure no command runs during the reset */
	mutex_lock(&us->dev_mutex);
}

static void storage_post_reset(struct usb_interface *iface)
{
	struct us_data *us = usb_get_intfdata(iface);

	/* Report the reset to the SCSI core */
	scsi_lock(us_to_host(us));
	scsi_report_bus_reset(us_to_host(us), 0);
	scsi_unlock(us_to_host(us));

	/* FIXME: Notify the subdrivers that they need to reinitialize
	* the device */
	mutex_unlock(&us->dev_mutex);
}


//Begin working...
static int storage_probe(struct usb_interface *intf,
						 const struct usb_device_id *id)
{
	struct Scsi_Host *host;
	struct us_data *us;
	int Ans;
	struct task_struct *th;
	int i;
	struct usb_endpoint_descriptor *ep;
	struct usb_endpoint_descriptor *ep_in = NULL;
	struct usb_endpoint_descriptor *ep_out = NULL;
	struct usb_endpoint_descriptor *ep_int = NULL;

	/*
	* Ask the SCSI layer to allocate a host structure, with extra
	* space at the end for our private us_data structure.
	*/
	host = scsi_host_alloc(&usb_stor_host_template, sizeof(*us));
	if (!host) {
		printk(KERN_WARNING	"allocate the scsi host failed\n");
		return -ENOMEM;
	}

	us = host_to_us(host);
	memset(us, 0, sizeof(struct us_data));
	mutex_init(&(us->dev_mutex));
	sema_init(&(us->sema),0);
	init_completion(&(us->notify));
	init_waitqueue_head(&us->delay_wait);

	/* Associate the us_data structure with the USB device */
	/* Fill in the device-related fields */
	us->pusb_dev = interface_to_usbdev(intf);
	us->pusb_intf = intf;
	us->ifnum = intf->cur_altsetting->desc.bInterfaceNumber;

	/* Store our private data in the interface */
	usb_set_intfdata(intf, us);

	/* Allocate the device-related DMA-mapped buffers */
	us->cr = usb_alloc_coherent(us->pusb_dev, sizeof(*us->cr),
		GFP_KERNEL, &us->cr_dma);
	if (!us->cr) {
		goto BadDevice;
	}

	us->iobuf = usb_alloc_coherent(us->pusb_dev, US_IOBUF_SIZE,
		GFP_KERNEL, &us->iobuf_dma);
	if (!us->iobuf) {

		goto BadDevice;
	}

	us->sensebuf = kmalloc(US_SENSE_SIZE, GFP_KERNEL);
	if (!us->sensebuf) {
		goto BadDevice;
	}

	us->subclass =	us->pusb_intf->cur_altsetting->desc.bInterfaceSubClass;	
	us->protocol =  us->pusb_intf->cur_altsetting->desc.bInterfaceProtocol ;
	us->flags = USB_US_ORIG_FLAGS(id->driver_info);

	if (us->flags & US_FL_IGNORE_DEVICE) {
		printk(KERN_INFO "device ignored\n");
		goto BadDevice;
	}

	/*
	* This flag is only needed when we're in high-speed, so let's
	* disable it if we're in full-speed
	*/
	if (us->pusb_dev->speed != USB_SPEED_HIGH)
		us->flags &= ~US_FL_GO_SLOW;


	/* Get the transport, protocol, and pipe settings */
//	if (us->protocol != US_PR_BULK){
//		printk(KERN_INFO "device mismatched\n");
//		goto BadDevice;
//	}
//
//	if (us->subclass != US_SC_8020){
//		printk(KERN_INFO "device mismatched\n");
//		goto BadDevice;
//	}

	us->max_lun = 0;
	/*
	* Find the endpoints we need.
	* We are expecting a minimum of 2 endpoints - in and out (bulk).
	* An optional interrupt is OK (necessary for CBI protocol).
	* We will ignore any others.
	*/
	for (i = 0; i < us->pusb_intf->cur_altsetting->desc.bNumEndpoints; i++) {
		ep = &us->pusb_intf->cur_altsetting->endpoint[i].desc;

		/* Is it a BULK endpoint? */
		if (usb_endpoint_xfer_bulk(ep)) {
			/* BULK in or out? */
			if (usb_endpoint_dir_in(ep))
				ep_in = ep;
			else
				ep_out = ep;
		}

		/* Is it an interrupt endpoint? */
		else if (usb_endpoint_xfer_int(ep)) {
			ep_int = ep;
		}
	}

	if (!ep_in || !ep_out) {
		goto BadDevice;	
	}

	/* Calculate and store the pipe values */
	us->send_ctrl_pipe = usb_sndctrlpipe(us->pusb_dev, 0);
	us->recv_ctrl_pipe = usb_rcvctrlpipe(us->pusb_dev, 0);
	us->send_bulk_pipe = usb_sndbulkpipe(us->pusb_dev,
		ep_out->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK);
	us->recv_bulk_pipe = usb_rcvbulkpipe(us->pusb_dev, 
		ep_in->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK);
	if (ep_int) {
		us->recv_intr_pipe = usb_rcvintpipe(us->pusb_dev,
			ep_int->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK);
		us->ep_bInterval = ep_int->bInterval;
	}


	//////
	us->current_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!us->current_urb) {
		goto BadDevice;	
	}

	th = kthread_create(usb_worker, us, "csk-usb-storage");
	if (IS_ERR(th)) {
		printk(KERN_WARNING "Unable to start control thread\n");
		goto BadDevice;	
	}

	/* Take a reference to the host for the control thread and
	* count it among all the threads we have launched.  Then
	* start it up. */
	scsi_host_get(us_to_host(us));
	atomic_inc(&g_total_workers);
	wake_up_process(th);	

	Ans = scsi_add_host(host, &intf->dev);
	if (Ans) {
		printk(KERN_WARNING "Unable to add the scsi host\n");
		goto BadDevice;
	}

	/////
	printk(KERN_DEBUG
		"SUMANG MOBILE device found at %d\n", us->pusb_dev->devnum);

	/* If the device is still connected, perform the scanning */
	if (!test_bit(US_FLIDX_DISCONNECTING, &us->flags)) {

		/* For bulk-only devices, determine the max LUN value */
		if (!(us->flags & US_FL_SINGLE_LUN)) {
			mutex_lock(&us->dev_mutex);
			/////

			/* issue the command */
			us->iobuf[0] = 0;
			Ans = usb_stor_control_msg(us, us->recv_ctrl_pipe,
				US_BULK_GET_MAX_LUN, 
				USB_DIR_IN | USB_TYPE_CLASS | 
				USB_RECIP_INTERFACE,
				0, us->ifnum, us->iobuf, 1, HZ);


			/* if we have a successful request, return the result */
			if (Ans > 0)
			{
				us->max_lun = us->iobuf[0];
			}
			
			if (Ans == -EPIPE) {
				usb_stor_clear_halt(us, us->recv_bulk_pipe);
				usb_stor_clear_halt(us, us->send_bulk_pipe);
			}



			////

			mutex_unlock(&us->dev_mutex);
		}
		scsi_scan_host(us_to_host(us));
		printk(KERN_DEBUG "device scan complete\n");

		/* Should we unbind if no devices were detected? */
	}

	scsi_host_put(us_to_host(us));

	scsi_host_get(us_to_host(us));

	/////
	return 0;

	/* We come here if there are any problems */
BadDevice:
	FreeUpAll(us);
	return -EPIPE;
}


//go home and have a rest..
static void storage_disconnect(struct usb_interface *i)
{
	struct us_data *us = usb_get_intfdata(i);

	quiesce_and_remove_host(us);
	FreeUpAll(us);

}

static struct usb_driver usb_storage_driver = {
	.name =		"csk-usb-storage",
	.probe =	storage_probe,
	.disconnect =	storage_disconnect,
	.pre_reset =	storage_pre_reset,
	.post_reset =	storage_post_reset,
	.id_table =	storage_usb_ids,
};

static int __init mod_init(void)
{
	int ans;

	if ((ans = usb_register(&usb_storage_driver)) == 0)
	{
		printk(KERN_INFO "CSK's USB Mass Storage driver,Loaded\n");
	}
	return ans;
}

static void __exit mod_exit(void)
{
	usb_deregister(&usb_storage_driver) ;
}

module_init(mod_init);
module_exit(mod_exit);




static void FreeUpAll(struct us_data *us)
{
	set_bit(US_FLIDX_DISCONNECTING, &us->flags);	//let our worker suspend
	up(&us->sema);
	usb_free_urb(us->current_urb);

	kfree(us->sensebuf);

	if (us->cr)
		usb_free_coherent(us->pusb_dev, sizeof(*us->cr), us->cr,
		us->cr_dma);
	if (us->iobuf)
		usb_free_coherent(us->pusb_dev, US_IOBUF_SIZE, us->iobuf,
		us->iobuf_dma);
	usb_set_intfdata(us->pusb_intf, NULL);

	scsi_host_put(us_to_host(us));
}

static void quiesce_and_remove_host(struct us_data *us)
{
	struct Scsi_Host *host = us_to_host(us);


	scsi_lock(host);
	set_bit(US_FLIDX_DISCONNECTING, &us->flags);
	scsi_unlock(host);

	usb_stor_stop_transport(us);
	wake_up(&us->delay_wait);


	mutex_lock(&us->dev_mutex);
	if (us->srb) {
		us->srb->result = DID_NO_CONNECT << 16;
		scsi_lock(host);
		us->srb->scsi_done(us->srb);
		us->srb = NULL;
		scsi_unlock(host);
	}
	mutex_unlock(&us->dev_mutex);

	/* Now we own no commands so it's safe to remove the SCSI host */
	scsi_remove_host(host);
}


/* Invoke the transport and basic error-handling/recovery methods
 *
 * This is used by the protocol layers to actually send the message to
 * the device and receive the response.
 */
static void usb_stor_invoke_transport(struct scsi_cmnd *srb, struct us_data *us)
{
	int need_auto_sense;
	int result;

	/* send the command to the transport layer */
	srb->resid = 0;
	result = usb_stor_Bulk_transport(srb, us);

	/* if the command gets aborted by the higher layers, we need to
	 * short-circuit all other processing
	 */
	if (test_bit(US_FLIDX_TIMED_OUT, &us->flags)) {
		
		srb->result = DID_ABORT << 16;
		goto Handle_Errors;
	}

	/* if there is a transport error, reset and don't auto-sense */
	if (result == USB_STOR_TRANSPORT_ERROR) {
		
		srb->result = DID_ERROR << 16;
		goto Handle_Errors;
	}

	/* if the transport provided its own sense data, don't auto-sense */
	if (result == USB_STOR_TRANSPORT_NO_SENSE) {
		srb->result = SAM_STAT_CHECK_CONDITION;
		return;
	}

	srb->result = SAM_STAT_GOOD;

	/* Determine if we need to auto-sense
	 *
	 * I normally don't use a flag like this, but it's almost impossible
	 * to understand what's going on here if I don't.
	 */
	need_auto_sense = 0;


	/*
	 * If we have a failure, we're going to do a REQUEST_SENSE 
	 * automatically.  Note that we differentiate between a command
	 * "failure" and an "error" in the transport mechanism.
	 */
	if (result == USB_STOR_TRANSPORT_FAILED) {
	
		need_auto_sense = 1;
	}

	/*
	 * A short transfer on a command where we don't expect it
	 * is unusual, but it doesn't mean we need to auto-sense.
	 */
	if ((srb->resid > 0) &&
	    !((srb->cmnd[0] == REQUEST_SENSE) ||
	      (srb->cmnd[0] == INQUIRY) ||
	      (srb->cmnd[0] == MODE_SENSE) ||
	      (srb->cmnd[0] == LOG_SENSE) ||
	      (srb->cmnd[0] == MODE_SENSE_10))) {
	
	}

	/* Now, if we need to do the auto-sense, let's do it */
	if (need_auto_sense) {
		int temp_result;
		void* old_request_buffer;
		unsigned short old_sg;
		unsigned old_request_bufflen;
		unsigned char old_sc_data_direction;
		unsigned char old_cmd_len;
		unsigned char old_cmnd[MAX_COMMAND_SIZE];
		int old_resid;

	
		/* save the old command */
		memcpy(old_cmnd, srb->cmnd, MAX_COMMAND_SIZE);
		old_cmd_len = srb->cmd_len;

		/* set the command and the LUN */
		memset(srb->cmnd, 0, MAX_COMMAND_SIZE);
		srb->cmnd[0] = REQUEST_SENSE;
		srb->cmnd[1] = old_cmnd[1] & 0xE0;
		srb->cmnd[4] = 18;

	
		srb->cmd_len = 12;

		/* set the transfer direction */
		old_sc_data_direction = srb->sc_data_direction;
		srb->sc_data_direction = DMA_FROM_DEVICE;

		/* use the new buffer we have */
		old_request_buffer = srb->request_buffer;
		srb->request_buffer = us->sensebuf;

		/* set the buffer length for transfer */
		old_request_bufflen = srb->request_bufflen;
		srb->request_bufflen = US_SENSE_SIZE;

		/* set up for no scatter-gather use */
		old_sg = srb->use_sg;
		srb->use_sg = 0;

		/* issue the auto-sense command */
		old_resid = srb->resid;
		srb->resid = 0;
		temp_result = usb_stor_Bulk_transport(us->srb, us);

		/* let's clean up right away */
		memcpy(srb->sense_buffer, us->sensebuf, US_SENSE_SIZE);
		srb->resid = old_resid;
		srb->request_buffer = old_request_buffer;
		srb->request_bufflen = old_request_bufflen;
		srb->use_sg = old_sg;
		srb->sc_data_direction = old_sc_data_direction;
		srb->cmd_len = old_cmd_len;
		memcpy(srb->cmnd, old_cmnd, MAX_COMMAND_SIZE);

		if (test_bit(US_FLIDX_TIMED_OUT, &us->flags)) {
			
			srb->result = DID_ABORT << 16;
			goto Handle_Errors;
		}
		if (temp_result != USB_STOR_TRANSPORT_GOOD) {
		

			/* we skip the reset if this happens to be a
			 * multi-target device, since failure of an
			 * auto-sense is perfectly valid
			 */
			srb->result = DID_ERROR << 16;
			if (!(us->flags & US_FL_SCM_MULT_TARG))
				goto Handle_Errors;
			return;
		}

	

		/* set the result so the higher layers expect this data */
		srb->result = SAM_STAT_CHECK_CONDITION;

		/* If things are really okay, then let's show that.  Zero
		 * out the sense buffer so the higher layers won't realize
		 * we did an unsolicited auto-sense. */
		if (result == USB_STOR_TRANSPORT_GOOD &&
			/* Filemark 0, ignore EOM, ILI 0, no sense */
				(srb->sense_buffer[2] & 0xaf) == 0 &&
			/* No ASC or ASCQ */
				srb->sense_buffer[12] == 0 &&
				srb->sense_buffer[13] == 0) {
			srb->result = SAM_STAT_GOOD;
			srb->sense_buffer[0] = 0x0;
		}
	}

	/* Did we transfer less than the minimum amount required? */
	if (srb->result == SAM_STAT_GOOD &&
			srb->request_bufflen - srb->resid < srb->underflow)
		srb->result = (DID_ERROR << 16) | (SUGGEST_RETRY << 24);

	return;

	/* Error and abort processing: try to resynchronize with the device
	 * by issuing a port reset.  If that fails, try a class-specific
	 * device reset. */
  Handle_Errors:

	/* Set the RESETTING bit, and clear the ABORTING bit so that
	 * the reset may proceed. */
	scsi_lock(us_to_host(us));
	set_bit(US_FLIDX_RESETTING, &us->flags);
	clear_bit(US_FLIDX_ABORTING, &us->flags);
	scsi_unlock(us_to_host(us));

	/* We must release the device lock because the pre_reset routine
	 * will want to acquire it. */
	mutex_unlock(&us->dev_mutex);
	result = usb_stor_port_reset(us);
	mutex_lock(&us->dev_mutex);

	if (result < 0) {
		scsi_lock(us_to_host(us));
		usb_stor_report_device_reset(us);
		scsi_unlock(us_to_host(us));
		usb_stor_Bulk_reset(us);
	}
	clear_bit(US_FLIDX_RESETTING, &us->flags);
}


static void usb_stor_stop_transport(struct us_data *us)
{


	/* If the state machine is blocked waiting for an URB,
	* let's wake it up.  The test_and_clear_bit() call
	* guarantees that if a URB has just been submitted,
	* it won't be cancelled more than once. */
	if (test_and_clear_bit(US_FLIDX_URB_ACTIVE, &us->flags)) {

		usb_unlink_urb(us->current_urb);
	}

	/* If we are waiting for a scatter-gather operation, cancel it. */
	if (test_and_clear_bit(US_FLIDX_SG_ACTIVE, &us->flags)) {

		usb_sg_cancel(&us->current_sg);
	}
}


static int usb_worker(void * __us)
{
	struct us_data *us = (struct us_data *)__us;
	struct Scsi_Host *host = us_to_host(us);

	current->flags |= PF_NOFREEZE;

	for(;;) {
		if(down_interruptible(&us->sema))
			break;

		/* lock the device pointers */
		mutex_lock(&(us->dev_mutex));

		/* if the device has disconnected, we are free to exit */
		if (test_bit(US_FLIDX_DISCONNECTING, &us->flags)) {
			mutex_unlock(&us->dev_mutex);
			break;
		}

		/* lock access to the state */
		scsi_lock(host);

		/* has the command timed out *already* ? */
		if (test_bit(US_FLIDX_TIMED_OUT, &us->flags)) {
			us->srb->result = DID_ABORT << 16;
			goto SkipForAbort;
		}

		scsi_unlock(host);

		/* reject the command if the direction indicator 
		* is UNKNOWN
		*/
		if (us->srb->sc_data_direction == DMA_BIDIRECTIONAL) {
			us->srb->result = DID_ERROR << 16;
		}

		/* reject if target != 0 or if LUN is higher than
		* the maximum known LUN
		*/
		else if (us->srb->device->id && 
			!(us->flags & US_FL_SCM_MULT_TARG)) {
				us->srb->result = DID_BAD_TARGET << 16;
		}

		else if (us->srb->device->lun > us->max_lun) {
			us->srb->result = DID_BAD_TARGET << 16;
		}

		/* Handle those devices which need us to fake 
		* their inquiry data */
		else  if ((us->srb->cmnd[0] == INQUIRY) &&
			(us->flags & US_FL_FIX_INQUIRY)) {
		}

		/* we've got a command, let's do it! */
		else {
	/* Pad the ATAPI command with zeros 
	*
	* NOTE: This only works because a scsi_cmnd struct field contains
	* a unsigned char cmnd[16], so we know we have storage available
	*/
	/* Pad the ATAPI command with zeros */

			for (; us->srb->cmd_len<12; us->srb->cmd_len++)
				us->srb->cmnd[us->srb->cmd_len] = 0;


			/* set command length to 12 bytes */
			us->srb->cmd_len = 12;

			/* send the command to the transport layer */
			usb_stor_invoke_transport(us->srb, us);
		}

		/* lock access to the state */
		scsi_lock(host);

		if (!us->srb)
			;		/* nothing to do */
		/* indicate that the command is done */
		else if (us->srb->result != DID_ABORT << 16) {
			us->srb->scsi_done(us->srb);
		} else {
		}

SkipForAbort:
		/* If an abort request was received we need to signal that
		* the abort has finished.  The proper test for this is
		* the TIMED_OUT flag, not srb->result == DID_ABORT, because
		* the timeout might have occurred after the command had
		* already completed with a different result code. */
		if (test_bit(US_FLIDX_TIMED_OUT, &us->flags)) {
			complete(&(us->notify));

			/* Allow USB transfers to resume */
			clear_bit(US_FLIDX_ABORTING, &us->flags);
			clear_bit(US_FLIDX_TIMED_OUT, &us->flags);
		}

		/* finished working on this command */
		us->srb = NULL;
		scsi_unlock(host);

		/* unlock the device pointers */
		mutex_unlock(&us->dev_mutex);
	} /* for (;;) */

	scsi_host_put(host);

	/* notify the exit routine that we're actually exiting now 
	*
	* complete()/wait_for_completion() is similar to up()/down(),
	* except that complete() is safe in the case where the structure
	* is getting deleted in a parallel mode of execution (i.e. just
	* after the down() -- that's necessary for the thread-shutdown
	* case.
	*
	*(struct bulk_cs_wrap *)bcb complete_and_exit() goes even further than this -- it is safe in
	* the case that the thread of the caller is going away (not just
	* the structure) -- this is necessary for the module-remove case.
	* This is important in preemption kernels, which transfer the flow
	* of execution immediately upon a complete().
	*/
	//complete_and_exit(&threads_gone, 0);
}	



static int usb_stor_clear_halt(struct us_data *us, unsigned int pipe)
{
	int result;
	int endp = usb_pipeendpoint(pipe);

	if (usb_pipein (pipe))
		endp |= USB_DIR_IN;

	result = usb_stor_control_msg(us, us->send_ctrl_pipe,
		USB_REQ_CLEAR_FEATURE, USB_RECIP_ENDPOINT,
		USB_ENDPOINT_HALT, endp,
		NULL, 0, 3*HZ);

	/* reset the endpoint toggle */
	if (result >= 0)
		usb_settoggle(us->pusb_dev, usb_pipeendpoint(pipe),
		usb_pipeout(pipe), 0);


	return result;
}


static int usb_stor_Bulk_transport(struct scsi_cmnd *srb, struct us_data *us)
{
	struct bulk_cb_wrap *bcb = (struct bulk_cb_wrap *) us->iobuf;
	struct bulk_cs_wrap *bcs = (struct bulk_cs_wrap *) us->iobuf;
	unsigned int transfer_length = srb->request_bufflen;
	unsigned int residue;
	int result;
	int fake_sense = 0;
	unsigned int cswlen;
	unsigned int cbwlen = US_BULK_CB_WRAP_LEN;


	/* set up the command wrapper */
	bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
	bcb->DataTransferLength = cpu_to_le32(transfer_length);
	bcb->Flags = srb->sc_data_direction == DMA_FROM_DEVICE ? 1 << 7 : 0;
	bcb->Tag = ++us->tag;
	bcb->Lun = srb->device->lun;
	if (us->flags & US_FL_SCM_MULT_TARG)
		bcb->Lun |= srb->device->id << 4;
	bcb->Length = srb->cmd_len;

	/* copy the command payload */
	memset(bcb->CDB, 0, sizeof(bcb->CDB));
	memcpy(bcb->CDB, srb->cmnd, bcb->Length);

	/* send it to out endpoint */
	
	result = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe,
				bcb, cbwlen, NULL);

	if (result != USB_STOR_XFER_GOOD)
		return USB_STOR_TRANSPORT_ERROR;

	/* DATA STAGE */
	/* send/receive data payload, if there is any */

	/* Some USB-IDE converter chips need a 100us delay between the
	 * command phase and the data phase.  Some devices need a little
	 * more than that, probably because of clock rate inaccuracies. */
	if (unlikely(us->flags & US_FL_GO_SLOW))
		udelay(125);

	if (transfer_length) {
		unsigned int pipe = srb->sc_data_direction == DMA_FROM_DEVICE ? 
				us->recv_bulk_pipe : us->send_bulk_pipe;
		result = usb_stor_bulk_transfer_sg(us, pipe,
					srb->request_buffer, transfer_length,
					srb->use_sg, &srb->resid);

		if (result == USB_STOR_XFER_ERROR)
			return USB_STOR_TRANSPORT_ERROR;

		/* If the device tried to send back more data than the
		 * amount requested, the spec requires us to transfer
		 * the CSW anyway.  Since there's no point retrying the
		 * the command, we'll return fake sense data indicating
		 * Illegal Request, Invalid Field in CDB.
		 */
		if (result == USB_STOR_XFER_LONG)
			fake_sense = 1;
	}

	/* See flow chart on pg 15 of the Bulk Only Transport spec for
	 * an explanation of how this code works.
	 */

	/* get CSW for device status */
	
	result = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe,
				bcs, US_BULK_CS_WRAP_LEN, &cswlen);

	/* Some broken devices add unnecessary zero-length packets to the
	 * end of their data transfers.  Such packets show up as 0-length
	 * CSWs.  If we encounter such a thing, try to read the CSW again.
	 */
	if (result == USB_STOR_XFER_SHORT && cswlen == 0) {
		
		result = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe,
				bcs, US_BULK_CS_WRAP_LEN, &cswlen);
	}

	/* did the attempt to read the CSW fail? */
	if (result == USB_STOR_XFER_STALLED) {

		/* get the status again */
		
		result = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe,
				bcs, US_BULK_CS_WRAP_LEN, NULL);
	}

	/* if we still have a failure at this point, we're in trouble */

	if (result != USB_STOR_XFER_GOOD)
		return USB_STOR_TRANSPORT_ERROR;

	/* check bulk status */
	residue = le32_to_cpu(bcs->Residue);
	
	if (bcs->Tag != us->tag || bcs->Status > US_BULK_STAT_PHASE) {
		
		return USB_STOR_TRANSPORT_ERROR;
	}

	/* Some broken devices report odd signatures, so we do not check them
	 * for validity against the spec. We store the first one we see,
	 * and check subsequent transfers for validity against this signature.
	 */
	if (!us->bcs_signature) {
		us->bcs_signature = bcs->Signature;

	} else if (bcs->Signature != us->bcs_signature) {
	
		return USB_STOR_TRANSPORT_ERROR;
	}

	/* try to compute the actual residue, based on how much data
	 * was really transferred and what the device tells us */
	if (residue) {
		if (!(us->flags & US_FL_IGNORE_RESIDUE)) {
			residue = min(residue, transfer_length);
			srb->resid = max(srb->resid, (int) residue);
		}
	}

	/* based on the status code, we report good or bad */
	switch (bcs->Status) {
		case US_BULK_STAT_OK:
			/* device babbled -- return fake sense data */
			if (fake_sense) {
				memcpy(srb->sense_buffer, 
				       usb_stor_sense_invalidCDB, 
				       sizeof(usb_stor_sense_invalidCDB));
				return USB_STOR_TRANSPORT_NO_SENSE;
			}

			/* command good -- note that data could be short */
			return USB_STOR_TRANSPORT_GOOD;

		case US_BULK_STAT_FAIL:
			/* command failed */
			return USB_STOR_TRANSPORT_FAILED;

		case US_BULK_STAT_PHASE:
			/* phase error -- note that a transport reset will be
			 * invoked by the invoke_transport() function
			 */
			return USB_STOR_TRANSPORT_ERROR;
	}

	/* we should never get here, but if we do, we're in trouble */
	return USB_STOR_TRANSPORT_ERROR;
}



/* Issue a USB port reset to the device.  The caller must not hold
 * us->dev_mutex.
 */
static int usb_stor_port_reset(struct us_data *us)
{
	int result, rc_lock;

	result = rc_lock =
		usb_lock_device_for_reset(us->pusb_dev, us->pusb_intf);
	if (result < 0)
		{}
	else {
		/* Were we disconnected while waiting for the lock? */
		if (test_bit(US_FLIDX_DISCONNECTING, &us->flags)) {
			result = -EIO;
		
		} else {
			result = usb_reset_composite_device(
					us->pusb_dev, us->pusb_intf);
			
		}
		if (rc_lock)
			usb_unlock_device(us->pusb_dev);
	}
	return result;
}

static void usb_stor_report_device_reset(struct us_data *us)
{
	int i;
	struct Scsi_Host *host = us_to_host(us);

	scsi_report_device_reset(host, 0, 0);
	if (us->flags & US_FL_SCM_MULT_TARG) {
		for (i = 1; i < host->max_id; ++i)
			scsi_report_device_reset(host, 0, i);
	}
}


/* This issues a Bulk-only Reset to the device in question, including
 * clearing the subsequent endpoint halts that may occur.
 */
static int usb_stor_Bulk_reset(struct us_data *us)
{
	
	int result;
	int result2;

	if (test_bit(US_FLIDX_DISCONNECTING, &us->flags)) {
		
		return -EIO;
	}

	result = usb_stor_control_msg(us, us->send_ctrl_pipe,
			US_BULK_RESET_REQUEST, USB_TYPE_CLASS | USB_RECIP_INTERFACE, 0,  us->ifnum, 0, 0,
			5*HZ);
	if (result < 0) {
		
		return result;
	}

 	/* Give the device some time to recover from the reset,
 	 * but don't delay disconnect processing. */
 	wait_event_interruptible_timeout(us->delay_wait,
 			test_bit(US_FLIDX_DISCONNECTING, &us->flags),
 			HZ*6);
	if (test_bit(US_FLIDX_DISCONNECTING, &us->flags)) {
		
		return -EIO;
	}

	
	result = usb_stor_clear_halt(us, us->recv_bulk_pipe);

	
	result2 = usb_stor_clear_halt(us, us->send_bulk_pipe);

	/* return a result code based on the result of the clear-halts */
	if (result >= 0)
		result = result2;
	
	return result;
}


/*
 * Transfer one buffer via bulk pipe, without timeouts, but allowing early
 * termination.  Return codes are USB_STOR_XFER_xxx.  If the bulk pipe
 * stalls during the transfer, the halt is automatically cleared.
 */
static int usb_stor_bulk_transfer_buf(struct us_data *us, unsigned int pipe,
	void *buf, unsigned int length, unsigned int *act_len)
{
	int result;

	/* fill and submit the URB */
	usb_fill_bulk_urb(us->current_urb, us->pusb_dev, pipe, buf, length,
		      usb_stor_blocking_completion, NULL);
	result = usb_stor_msg_common(us, 0);

	/* store the actual length of the data transferred */
	if (act_len)
		*act_len = us->current_urb->actual_length;
	return interpret_urb_result(us, pipe, length, result, 
			us->current_urb->actual_length);
}


/*
 * Transfer an entire SCSI command's worth of data payload over the bulk
 * pipe.
 *
 * Note that this uses usb_stor_bulk_transfer_buf() and
 * usb_stor_bulk_transfer_sglist() to achieve its goals --
 * this function simply determines whether we're going to use
 * scatter-gather or not, and acts appropriately.
 */
static int usb_stor_bulk_transfer_sg(struct us_data* us, unsigned int pipe,
		void *buf, unsigned int length_left, int use_sg, int *residual)
{
	int result;
	unsigned int partial;

	/* are we scatter-gathering? */
	if (use_sg) {
		/* use the usb core scatter-gather primitives */
		result = usb_stor_bulk_transfer_sglist(us, pipe,
				(struct scatterlist *) buf, use_sg,
				length_left, &partial);
		length_left -= partial;
	} else {
		/* no scatter-gather, just make the request */
		result = usb_stor_bulk_transfer_buf(us, pipe, buf, 
				length_left, &partial);
		length_left -= partial;
	}

	/* store the residual and return the error code */
	if (residual)
		*residual = length_left;
	return result;
}



/*
 * Transfer one control message, with timeouts, and allowing early
 * termination.  Return codes are usual -Exxx, *not* USB_STOR_XFER_xxx.
 */
static int usb_stor_control_msg(struct us_data *us, unsigned int pipe,
		 u8 request, u8 requesttype, u16 value, u16 index, 
		 void *data, u16 size, int timeout)
{
	int status;


	/* fill in the devrequest structure */
	us->cr->bRequestType = requesttype;
	us->cr->bRequest = request;
	us->cr->wValue = cpu_to_le16(value);
	us->cr->wIndex = cpu_to_le16(index);
	us->cr->wLength = cpu_to_le16(size);

	/* fill and submit the URB */
	usb_fill_control_urb(us->current_urb, us->pusb_dev, pipe, 
			 (unsigned char*) us->cr, data, size, 
			 usb_stor_blocking_completion, NULL);
	status = usb_stor_msg_common(us, timeout);

	/* return the actual length of the data transferred if no error */
	if (status == 0)
		status = us->current_urb->actual_length;
	return status;
}

/* This is the common part of the URB message submission code
 *
 * All URBs from the usb-storage driver involved in handling a queued scsi
 * command _must_ pass through this function (or something like it) for the
 * abort mechanisms to work properly.
 */
static int usb_stor_msg_common(struct us_data *us, int timeout)
{
	struct completion urb_done;
	long timeleft;
	int status;

	/* don't submit URBs during abort/disconnect processing */
	if (us->flags & ABORTING_OR_DISCONNECTING)
		return -EIO;

	/* set up data structures for the wakeup system */
	init_completion(&urb_done);

	/* fill the common fields in the URB */
	us->current_urb->context = &urb_done;
	us->current_urb->actual_length = 0;
	us->current_urb->error_count = 0;
	us->current_urb->status = 0;


	us->current_urb->transfer_flags = URB_NO_SETUP_DMA_MAP;
	if (us->current_urb->transfer_buffer == us->iobuf)
		us->current_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
	us->current_urb->transfer_dma = us->iobuf_dma;
	us->current_urb->setup_dma = us->cr_dma;

	/* submit the URB */
	status = usb_submit_urb(us->current_urb, GFP_NOIO);
	if (status) {
		/* something went wrong */
		return status;
	}

	/* since the URB has been submitted successfully, it's now okay
	 * to cancel it */
	set_bit(US_FLIDX_URB_ACTIVE, &us->flags);

	/* did an abort/disconnect occur during the submission? */
	if (us->flags & ABORTING_OR_DISCONNECTING) {

		/* cancel the URB, if it hasn't been cancelled already */
		if (test_and_clear_bit(US_FLIDX_URB_ACTIVE, &us->flags)) {
		
			usb_unlink_urb(us->current_urb);
		}
	}
 
	/* wait for the completion of the URB */
	timeleft = wait_for_completion_interruptible_timeout(
			&urb_done, timeout ? : MAX_SCHEDULE_TIMEOUT);
 
	clear_bit(US_FLIDX_URB_ACTIVE, &us->flags);

	if (timeleft <= 0) {
		usb_kill_urb(us->current_urb);
	}

	/* return the URB status */
	return us->current_urb->status;
}

/*
 * Interpret the results of a URB transfer
 *
 * This function prints appropriate debugging messages, clears halts on
 * non-control endpoints, and translates the status to the corresponding
 * USB_STOR_XFER_xxx return code.
 */
static int interpret_urb_result(struct us_data *us, unsigned int pipe,
		unsigned int length, int result, unsigned int partial)
{
	
	switch (result) {

	/* no error code; did we send all the data? */
	case 0:
		if (partial != length) {
		
			return USB_STOR_XFER_SHORT;
		}

		
		return USB_STOR_XFER_GOOD;

	/* stalled */
	case -EPIPE:
		/* for control endpoints, (used by CB[I]) a stall indicates
		 * a failed command */
		if (usb_pipecontrol(pipe)) {
		
			return USB_STOR_XFER_STALLED;
		}

		/* for other sorts of endpoint, clear the stall */
		
		if (usb_stor_clear_halt(us, pipe) < 0)
			return USB_STOR_XFER_ERROR;
		return USB_STOR_XFER_STALLED;

	/* babble - the device tried to send more than we wanted to read */
	case -EOVERFLOW:
		
		return USB_STOR_XFER_LONG;

	/* the transfer was cancelled by abort, disconnect, or timeout */
	case -ECONNRESET:
		
		return USB_STOR_XFER_ERROR;

	/* short scatter-gather read transfer */
	case -EREMOTEIO:
	
		return USB_STOR_XFER_SHORT;

	/* abort or disconnect in progress */
	case -EIO:
	
		return USB_STOR_XFER_ERROR;

	/* the catch-all error case */
	default:
	
		return USB_STOR_XFER_ERROR;
	}
}


static int usb_stor_bulk_transfer_sglist(struct us_data *us, unsigned int pipe,
		struct scatterlist *sg, int num_sg, unsigned int length,
		unsigned int *act_len)
{
	int result;

	/* don't submit s-g requests during abort/disconnect processing */
	if (us->flags & ABORTING_OR_DISCONNECTING)
		return USB_STOR_XFER_ERROR;

	/* initialize the scatter-gather request block */
	
	result = usb_sg_init(&us->current_sg, us->pusb_dev, pipe, 0,
			sg, num_sg, length, GFP_NOIO);
	if (result) {
	
		return USB_STOR_XFER_ERROR;
	}

	/* since the block has been initialized successfully, it's now
	 * okay to cancel it */
	set_bit(US_FLIDX_SG_ACTIVE, &us->flags);

	/* did an abort/disconnect occur during the submission? */
	if (us->flags & ABORTING_OR_DISCONNECTING) {

		/* cancel the request, if it hasn't been cancelled already */
		if (test_and_clear_bit(US_FLIDX_SG_ACTIVE, &us->flags)) {
			
			usb_sg_cancel(&us->current_sg);
		}
	}

	/* wait for the completion of the transfer */
	usb_sg_wait(&us->current_sg);
	clear_bit(US_FLIDX_SG_ACTIVE, &us->flags);

	result = us->current_sg.status;
	if (act_len)
		*act_len = us->current_sg.bytes;
	return interpret_urb_result(us, pipe, length, result,
			us->current_sg.bytes);
}

/* Determine what the maximum LUN supported is */
static int usb_stor_Bulk_max_lun(struct us_data *us)
{
	int result;

	/* issue the command */
	us->iobuf[0] = 0;
	result = usb_stor_control_msg(us, us->recv_ctrl_pipe,
				 US_BULK_GET_MAX_LUN, 
				 USB_DIR_IN | USB_TYPE_CLASS | 
				 USB_RECIP_INTERFACE,
				 0, us->ifnum, us->iobuf, 1, HZ);

	
	/* if we have a successful request, return the result */
	if (result > 0)
		return us->iobuf[0];

	if (result == -EPIPE) {
		usb_stor_clear_halt(us, us->recv_bulk_pipe);
		usb_stor_clear_halt(us, us->send_bulk_pipe);
	}

	return 0;
}


static void usb_stor_blocking_completion(struct urb *urb)
{
	struct completion *urb_done_ptr = (struct completion *)urb->context;

	complete(urb_done_ptr);
}


/* Output routine for the sysfs max_sectors file */
static ssize_t show_max_sectors(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct scsi_device *sdev = to_scsi_device(dev);
	printk("show_max_sectors:%u\n",sdev->request_queue->max_sectors);

	return sprintf(buf, "%u\n", sdev->request_queue->max_sectors);
}

/* Input routine for the sysfs max_sectors file */
static ssize_t store_max_sectors(struct device *dev, struct device_attribute *attr, const char *buf,
		size_t count)
{
	struct scsi_device *sdev = to_scsi_device(dev);
	unsigned short ms;

	if (sscanf(buf, "%hu", &ms) > 0 && ms <= SCSI_DEFAULT_MAX_SECTORS) {
		blk_queue_max_sectors(sdev->request_queue, ms);
		return strlen(buf);
	}
	return -EINVAL;	
}



static int proc_info (struct Scsi_Host *host, char *buffer,
		char **start, off_t offset, int length, int inout)
{
	struct us_data *us = host_to_us(host);
	char *pos = buffer;
	const char *string;

	/* if someone is sending us data, just throw it away */
	if (inout)
		return length;

	/* print the controller name */
	SPRINTF("   Host scsi%d: usb-storage\n", host->host_no);


	string = us->pusb_dev->manufacturer;

	SPRINTF("       Vendor: %s\n", string);

	string = us->pusb_dev->product;
	SPRINTF("      Product: %s\n", string);

	string = "None";
	SPRINTF("Serial Number: %s\n", string);
	/* show the protocol and transport */


	/* show the device flags */
	if (pos < buffer + length) {
		pos += sprintf(pos, "       Quirks:");
		*(pos++) = '\n';
	}

	/*
	 * Calculate start of next buffer, and return value.
	 */
	*start = buffer + offset;

	if ((pos - buffer) < offset)
		return (0);
	else if ((pos - buffer - offset) < length)
		return (pos - buffer - offset);
	else
		return (length);
}

static const char* host_info(struct Scsi_Host *host)
{
	return "SCSI emulation for SAMSUNG mobile";
}

static int queuecommand(struct scsi_cmnd *srb,
			void (*done)(struct scsi_cmnd *))
{
	struct us_data *us = host_to_us(srb->device->host);

	if (us->srb != NULL) {
		printk(KERN_ERR "Error in %s: us->srb = %p\n",
			__FUNCTION__, us->srb);
		return SCSI_MLQUEUE_HOST_BUSY;
	}

	
	if (test_bit(US_FLIDX_DISCONNECTING, &us->flags)) {
	
		srb->result = DID_NO_CONNECT << 16;
		done(srb);
		return 0;
	}

	/* enqueue the command and wake up the control thread */
	srb->scsi_done = done;
	us->srb = srb;
	up(&(us->sema));

	return 0;
}

static int command_abort(struct scsi_cmnd *srb)
{
	struct us_data *us = host_to_us(srb->device->host);


	scsi_lock(us_to_host(us));

	/* Is this command still active? */
	if (us->srb != srb) {
		scsi_unlock(us_to_host(us));
		return FAILED;
	}


	set_bit(US_FLIDX_TIMED_OUT, &us->flags);
	if (!test_bit(US_FLIDX_RESETTING, &us->flags)) {
		set_bit(US_FLIDX_ABORTING, &us->flags);
		usb_stor_stop_transport(us);
	}
	scsi_unlock(us_to_host(us));

	/* Wait for the aborted command to finish */
	wait_for_completion(&us->notify);
	return SUCCESS;
}

static int device_reset(struct scsi_cmnd *srb)
{
	struct us_data *us = host_to_us(srb->device->host);
	int result;

	/* lock the device pointers and do the reset */
	mutex_lock(&(us->dev_mutex));
	result = usb_stor_Bulk_reset(us);
	mutex_unlock(&us->dev_mutex);

	return result < 0 ? FAILED : SUCCESS;
}

/* Simulate a SCSI bus reset by resetting the device's USB port. */
static int bus_reset(struct scsi_cmnd *srb)
{
	struct us_data *us = host_to_us(srb->device->host);
	int result;

	result = usb_stor_port_reset(us);
	return result < 0 ? FAILED : SUCCESS;
}

static int slave_alloc (struct scsi_device *sdev)
{
	struct us_data *us = host_to_us(sdev->host);

	sdev->inquiry_len = 36;

	return 0;
}

static int slave_configure(struct scsi_device *sdev)
{
	struct us_data *us = host_to_us(sdev->host);


	blk_queue_dma_alignment(sdev->request_queue, (512 - 1));

	
	if (sdev->scsi_level < SCSI_2)
		sdev->scsi_level = sdev->sdev_target->scsi_level = SCSI_2;

	
	if ((us->flags & US_FL_MAX_SECTORS_64) &&
			sdev->request_queue->max_sectors > 64)
		blk_queue_max_sectors(sdev->request_queue, 64);

	
	if (sdev->type == TYPE_DISK) {

		
			sdev->use_10_for_ms = 1;

		
		sdev->use_192_bytes_for_3f = 1;

		if (us->flags & US_FL_NO_WP_DETECT)
			sdev->skip_ms_page_3f = 1;

		
		sdev->skip_ms_page_8 = 1;

		
		if (us->flags & US_FL_FIX_CAPACITY)
			sdev->fix_capacity = 1;

		
		sdev->scsi_level = sdev->sdev_target->scsi_level = SCSI_2;

		
		sdev->retry_hwerror = 1;

	} else {

		
		sdev->use_10_for_ms = 1;
	}

	
	if (us->flags & US_FL_NOT_LOCKABLE)
		sdev->lockable = 0;

	
	return 0;
}

