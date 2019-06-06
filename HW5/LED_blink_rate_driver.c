/*****************************
Abdullah Barghouti 
ECE 373
HW 5 - blinking LED kernel driver 
Portland State University 2019
******************************/

#include <linux/init.h>
#include <linux/kernel.h>
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
#include <linux/netdevice.h>
#include <linux/device.h>
#include <linux/timekeeping.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/timer.h>


#define DEVCNT 1
#define DEVNAME "blink_driver"
#define NODE_NAME "ece_led"
#define LEDCONTROLREGISTER  0xE00

#define LED_ZERO_OFF 	0xF
#define LED_ZERO_ON		0xE

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
}	myPCI;

static struct timer_list my_timer;
static int blink_rate = 2;
unsigned int half_duty = 2000;

// insmod *,ko blink_rate=#
module_param(blink_rate, int, S_IRUSR | S_IWUSR);

void timer_callback(struct timer_list *list)
{
	int ret;
	mydev.led_reg_val = ioread32(myPCI.hw_addr + LEDCONTROLREGISTER);
	//printk("value recieved %lx\n", mydev.led_reg_val);

	if(mydev.led_reg_val == LED_ZERO_OFF)
	{
		//turn LED on 
		mydev.led_reg_val = LED_ZERO_ON;
		
		//write value to ctrl register 
		iowrite32(mydev.led_reg_val, myPCI.hw_addr + LEDCONTROLREGISTER);
		//printk(KERN_INFO "LED ON value set and written \n");
	
	}
	else
	{
		mydev.led_reg_val = LED_ZERO_OFF;
		iowrite32(mydev.led_reg_val, myPCI.hw_addr + LEDCONTROLREGISTER);
		//printk(KERN_INFO "LED OFF value set and written \n");
	
	}

	if (blink_rate <= 0)
	{
		//printk(KERN_ERR "blink_rate was zero or negative");
		blink_rate = 2;
		printk(KERN_INFO"blink_rate changed to %d", blink_rate);
		mod_timer(&my_timer, jiffies + msecs_to_jiffies(1000));

	}
	else 
	{
		mod_timer(&my_timer, jiffies + msecs_to_jiffies(half_duty/blink_rate));
	
	}

}


//open
static int char_driver_open(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "open module succeful \n");
	
	if(blink_rate <=0)
	{
		blink_rate = 2;
		half_duty = 1000;
	}
	else 
	{
		half_duty = 2000/blink_rate;
	}
	printk(KERN_INFO "blink_rate check val \n");
	mod_timer(&my_timer, jiffies + msecs_to_jiffies(half_duty));
	
	//turn LED ON
	mydev.led_reg_val = LED_ZERO_ON;
	iowrite32(mydev.led_reg_val, myPCI.hw_addr + LEDCONTROLREGISTER);
	printk(KERN_INFO "iowrite in open \n");
	return 0;
}


//read
static ssize_t char_driver_read(struct file *file, char __user *buf, size_t len, loff_t *offset){
	int ret;
	printk(KERN_INFO "Currently Reading...\n");


	if(*offset >= sizeof(int))
		return 0;	

	if(!buf){
		ret = -EINVAL;
		goto out;
	}

	if(copy_to_user(buf, &blink_rate, sizeof(int))){
		ret = -EFAULT;
		goto out;
	}
	ret = sizeof(int);
	*offset += len;

	/* Good to go, so printk the thingy */
	printk(KERN_INFO "User got from us %d\n", blink_rate);

out:
	return ret;
}

//function to write/ update based on userspace
static ssize_t char_driver_write(struct file *file, const char __user *buf, size_t len, loff_t *offset){
	int ret;
	char *kern_buf;

	printk(KERN_INFO "Currently Writing...\n");
	
	if(!buf){
		ret = -EINVAL;
		goto out;
	}

	kern_buf = kmalloc(len,GFP_KERNEL);
	if(!kern_buf){
		ret = -ENOMEM;
		goto out;
	}
	if(copy_from_user(kern_buf, buf, len)){
		ret = -EFAULT;
		printk(KERN_ERR "copy_from_user blink_rate failed");
		goto mem_free;
	}

	ret = len;

	printk(KERN_INFO "userspace wrote %d \n", *kern_buf);

	

	if (*kern_buf <= 0)
	{
		ret = -EINVAL;
		printk(KERN_ERR "blink_rate was zero or negative");
		*kern_buf = 2;
		printk(KERN_INFO"blink_rate changed to %d", *kern_buf);

	}
	else 
	{
		blink_rate = *kern_buf;
	}

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
