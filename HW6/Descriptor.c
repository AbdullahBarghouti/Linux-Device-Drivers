/*****************************
Abdullah Barghouti 
ECE 373
HW 6 - Descriptor Interrupt kernel driver 
Portland State University 2019
******************************/

#include <linux/init.h>

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>


#define DEVCNT 1
#define DEVNAME "ece_led"
#define NODE_NAME "ece_led"


#define LED_ZERO_OFF 			0xF
#define LED_ZERO_ON				0xE
#define RING_BUFF_SIZE 				16

//some more pci driver offsets and vals
#define DEV_STATUS_REG			0x00008
#define CNTRL_LINK_UP			0x1A41
#define LED_COTRL_REG		  	0xE00

//interrupt
#define IRQ_EN	 				0x10
#define IMC 					0x000D8
#define IMS 					0x000D0
#define ICR 					0x000C0

//recieve register 
#define R_CNTRL_REG				0x00100
#define R_SETUP					0x801A
#define R_RDBAL					0x02800
#define R_RDBAH					0x02804
#define R_LEN 					0x02808
#define R_HEAD					0x02810
#define R_TAIL					0x02818

//descriptor 
//https://elixir.bootlin.com/linux/latest/source/drivers/net/ethernet/intel/e1000/e1000.h
//#define E1000_RX_DESC_EXT(R, i) (&(((union e1000_rx_desc_extended *)((R).desc))[i]))
#define E1000_GET_DESC(R, i, type) (&(((struct type *)((R).desc))[i]))
#define E1000_RX_DESC(R, i)	E1000_GET_DESC(R, i, e1000_rx_desc)

//receive Descriptor
struct e1000_rx_desc {
	__le64 buffer_addr;		/* Address of the descriptor's data buffer */
	__le16 length;			/* Length of data DMAed into data buffer */
	__le16 csum;			/* Packet checksum */
	u8 status;				/* Descriptor status */
	u8 errors;				/* Descriptor Errors */
	__le16 special;
}*rx_desc;


struct mydev_rx_ring {
    struct mydev_dev *mydev;        /* pointer to ring memory  */
    void *desc;                		/* pointer to Legacy descriptor */
    struct buffer *buf;            	/* pointer to buffer struct */
    dma_addr_t dma;                	/* physical address of ring */
    unsigned int size;            	/* length of ring in bytes */
    unsigned int count;           	/* number of desc. in ring */
    void  *head;
    void  *tail;
}*rx_ring;

char blinkDriver_name[] = DEVNAME;


static struct mydev_dev{
	struct cdev cdev;
	dev_t mydev_node;
	int sys_int;
	int syscall_val;
	long led_inital_val;
	long led_reg_val;
	struct class *class;
} mydev;

static struct myPCI {
	struct pci_dev *pdev;
	void *hw_addr;
	struct work_struct ws;
}	myPCI;


/***************/

// insmod *,ko blink_rate=#
//module_param(blink_rate, int, S_IRUSR | S_IWUSR);

//open
static int char_driver_open(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "open module succeful \n");
	return 0;
}

//read
static ssize_t char_driver_read(struct file *file, char __user *buf, size_t len, loff_t *offset){
	int ret;
	uint32_t head = readl(myPCI.hw_addr + R_HEAD);
	uint32_t tail = readl(myPCI.hw_addr + R_TAIL);
	printk(KERN_INFO "Currently Reading...\n");

	uint32_t combiend;
	combined = head;
	combined <<= 16;
	combined = tail;

	printk(KERN_INFO "CURRENT head and tail %08x\n", combined);
	if(*offset >= sizeof(uint32_t))
		return 0;	

	if(!buf){
		ret = -EINVAL;
		goto out;
	}

	if(copy_to_user(buf, &combined, sizeof(uint32_t))){
		ret = -EFAULT;
		goto out;
	}
	ret = sizeof(uint32_t);
	*offset += len;

	printk(KERN_INFO "READ head and tail %08x\n", combined);

out:
	return ret;
}

//function to write/ update based on userspace
static ssize_t char_driver_write(struct file *file, const char __user *buf, size_t len, loff_t *offset){
	int ret;
	printk(KERN_INFO "Currently Writing...\n");
	
	if(!buf){
		ret = -EINVAL;
		goto out;
	}

	if(copy_from_user(&ret, buf, len)){
		ret = -EFAULT;
		printk(KERN_ERR "copy_from_user blink_rate failed");
		goto mem_free;
	}
	
	ret = len;

	printk(KERN_INFO "userspace wrote %d \n", ret);
	printk(KERN_INFO "iowrite done!");

out:
	return ret;
mem_free:
	kfree(kern_buf);

}

static int pci_networkLEDBlinkDriver_probe(struct pci_dev *pdev, const struct pci_device_id *ent){
	resource_size_t mmio_start, mmio_len; //memory mapped IO
	unsigned long barMask;
	printk(KERN_INFO "pci_networkLEDBlinkDriver_probe called \n");
	
	//get BAR mask  - base address register 
	barMask = pci_select_bars(pdev, IORESOURCE_MEM);
	printk(KERN_INFO "barMask %lx", barMask);
	
	//reserve BAR areas
	if(pci_request_selected_regions(pdev, barMask, blinkDriver_name)) 
	{
		printk(KERN_INFO "pci_request_selected_regions failed\n");
		goto unregister_selected_regions;
	};
	
	mmio_start = pci_resource_start(pdev, 0);
	mmio_len = pci_resource_len(pdev, 0);

	printk(KERN_INFO "mmio start: %lx", (unsigned long) mmio_start);
	printk(KERN_INFO "mmio len: %lx", (unsigned long) mmio_len);
	
	// ioremap is designed specifcally to assing virtual addresses 
	// to I/O memory regions since many I/O systems cannot be 
	if (!(myPCI.hw_addr = ioremap(mmio_start, mmio_len)))
	{
		printk(KERN_INFO "iomap failed \n");
		goto unregister_ioremap;
	}

	mydev.led_inital_val = ioread32(myPCI.hw_addr + 0xE00);
	printk(KERN_INFO "inital val is %lx\n", mydev.led_inital_val);

	return 0;

unregister_ioremap:
	iounmap(myPCI.hw_addr);

unregister_selected_regions:
	pci_release_selected_regions(pdev, pci_select_bars(pdev, IORESOURCE_MEM));

	return -1;
};

static void pci_networkLEDBlinkDriver_remove(struct pci_dev *pdev){
	printk(KERN_INFO "pci_networkLEDBlinkDriver_remove called\n");
	iounmap(myPCI.hw_addr);
	pci_release_selected_regions(pdev, pci_select_bars(pdev, IORESOURCE_MEM));
}

// open, read, write struct 
static struct file_operations mydev_fops = {
	.owner 	= THIS_MODULE,
	.open 	= char_driver_open,
	.read 	= char_driver_read,
	.write 	= char_driver_write,
};	

static const struct pci_device_id pci_networkLEDBlinkDriver_table[] = {

	{ PCI_DEVICE(0x8086, 0x100e) },

	/* required last entry */
	{0, }
};


//PCI name, id_table, probe, remove 
//callback functions
static struct pci_driver pci_networkLEDBlinkDriver = {
	.name 		= "blink driver",
	.id_table 	= pci_networkLEDBlinkDriver_table,
	.probe 		= pci_networkLEDBlinkDriver_probe,  
	.remove		= pci_networkLEDBlinkDriver_remove,
};
		

// init function
static int __init char_driver_init(void){
	printk(KERN_INFO "Module loading... \n");	
 
	// get major and minor device numbers 
	if(alloc_chrdev_region(&mydev.mydev_node, 0, DEVCNT, DEVNAME)){
		printk(KERN_ERR "Allocating chrdev failed \n");
		return -1;
	}

	printk(KERN_INFO "Allocated %d devices at major: %d\n", DEVCNT, MAJOR(mydev.mydev_node));

	//initialize the character device 
	cdev_init(&mydev.cdev, &mydev_fops);
	mydev.cdev.owner = THIS_MODULE;

	if(cdev_add(&mydev.cdev, mydev.mydev_node, DEVCNT)){
		printk(KERN_ERR "cdev_add failed \n");
		/* clean up allocation of chrdev */
		unregister_chrdev_region(mydev.mydev_node, DEVCNT);
		return -1;
	}

	//register as a pci driver 
	if(pci_register_driver(&pci_networkLEDBlinkDriver)){
			printk(KERN_ERR "pci_register_driver failed \n");
			pci_unregister_driver(&pci_networkLEDBlinkDriver);
	}
	
	//create dev node
	if((mydev.class = class_create(THIS_MODULE,NODE_NAME)) == NULL)
	{
		printk(KERN_ERR "class_create failed\n");
		class_destroy(mydev.class);
	}

	if(device_create(mydev.class, NULL, mydev.mydev_node, NULL,NODE_NAME) == NULL)
	{
		printk(KERN_ERR "device_create failed\n");
		device_destroy(mydev.class, mydev.mydev_node);
	}
	timer_setup(&my_timer, timer_callback, 0);

	return 0;
}

// exit function 
static void __exit char_driver_exit(void){
	//clean up in order from last to first 
	pci_unregister_driver(&pci_networkLEDBlinkDriver);
	device_destroy(mydev.class, mydev.mydev_node);
	class_destroy(mydev.class);
	cdev_del(&mydev.cdev);
	unregister_chrdev_region(mydev.mydev_node, DEVCNT);
	del_timer_sync(&my_timer);
	printk(KERN_INFO "Unloaded module \n");
}

MODULE_AUTHOR("Abdullah Barghouti");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.2");
module_init(char_driver_init); // format of macro function is
module_exit(char_driver_exit); // module_param(var, type, flags);
							   // type is int or char 							   
							   // flags determine if you want to expose it in sysfs (write, read)
