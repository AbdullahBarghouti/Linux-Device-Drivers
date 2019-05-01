/*
Abdullah Barghouti 
ECE 373 HW 3
Portland State University 2019
*/

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
/* NEW FOR HOMEWORK 3*/
#include <linux/pci.h>
#include <linux/netdevice.h>
/*	*/

#define DEVCNT 1
#define DEVNAME "blinkDriver"
char blinkDriver_name[] = "blinkDriver";

static struct mydev_dev{
	struct cdev cdev;
	dev_t mydev_node;
	int sys_int;
	int syscall_val;
	long led_inital_val;
} mydev;

/* NEW FOR HOMEWORK 3*/
static struct myPCI {
	struct pci_dev *pdev;
	void *hw_addr;
}	myPCI;
/*		*/


static int sys_intial_value = 40;
module_param(sys_intial_value, int, S_IRUSR | S_IWUSR);

// function for opening module 
static int char_driver_open(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "open module succeful \n");
	mydev.syscall_val = sys_intial_value;
	//mydev.sys_int = 40;
	return 0;
}

// function for reading 
static ssize_t char_driver_read(struct file *file, char __user *buf, size_t len, loff_t *offset){
	int ret;
	printk(KERN_INFO "Currently Reading...");

	if(*offset >= sizeof(int))
		return 0;	
	if(!buf){
		ret = -EINVAL;
		goto out;
	}
	if(copy_to_user(buf, &mydev.syscall_val, sizeof(int))){
		ret = -EFAULT;
		goto out;
	}
	ret = sizeof(int);
	*offset += len;

	/* Good to go, so printk the thingy */
	printk(KERN_INFO "User got from us %d\n", mydev.sys_int);
	
out:
	return ret;
}

//function to write/ update based on userspace
static ssize_t char_driver_write(struct file *file, const char __user *buf, size_t len, loff_t *offset){
	
	char *kern_buf;
	int ret;
	printk(KERN_INFO "Currently Writing...");
	
	if(!buf){
		ret = -EINVAL;
		goto out;
	}
	kern_buf = kmalloc(len, GFP_KERNEL);
	if(!kern_buf){
		ret = -ENOMEM;
		goto out;
	}
	if(copy_from_user(kern_buf, buf, len)){
		ret = -EFAULT;
		goto mem_out;
	}
	ret = len;
	printk(KERN_INFO "Userspace wrote \"%s\" to us \n", kern_buf);
mem_out:
	kfree(kern_buf);
out:
	return ret;
}
/*NEW FOR HW3 */
static int pci_networkLEDBlinkDriver_probe(struct pci_dev *pdev, const struct pci_device_id *ent){
	resource_size_t mmio_start, mmio_len;
	unsigned long barMask;
	printk(KERN_INFO "pci_networkLEDBlinkDriver_probe called \n");
	//get BAR mask 
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

	if (!(myPCI.hw_addr = ioremap(mmio_start, mmio_len)))
	{
		printk(KERN_INFO "iomap failed \n");
		goto unregister_ioremap;
	}

	mydev.led_inital_val = readl(myPCI.hw_addr + 0xE00);
	printk(KERN_INFO "inital val is %lx\n", mydev.led_inital_val);

	return 0;

unregister_ioremap:
	iounmap(myPCI.hw_addr);

unregister_selected_regions:
	pci_release_selected_regions(pdev, pci_select_bars(pdev, IORESOURCE_MEM));

};

static void pci_networkLEDBlinkDriver_remove(struct pci_dev *pdev){
	printk(KERN_INFO "pci_networkLEDBlinkDriver_remove called\n");
	iounmap(myPCI.hw_addr);
	pci_release_selected_regions(pdev, pci_select_bars(pdev, IORESOURCE_MEM));
}

/*		*/
// open, read, write struct 
static struct file_operations mydev_fops = {
	.owner 	= THIS_MODULE,
	.open 	= char_driver_open,
	.read 	= char_driver_read,
	.write 	= char_driver_write,
};	

/* NEW FOR HOMEWORK 3*/
static const struct pci_device_id pci_networkLEDBlinkDriver_table[] = {

	{ PCI_DEVICE(0x8086, 0x100e) },

	/* required last entry */
	{0, }
};
/*			*/

//PCI name, id_table, probe, remove 
/* NEW FOR HOMEWORK 3*/
//callback functions
static struct pci_driver pci_networkLEDBlinkDriver = {
	.name 		= "Blink Driver - HW3",
	.id_table 	= pci_networkLEDBlinkDriver_table,
	.probe 		= pci_networkLEDBlinkDriver_probe,  
	.remove		= pci_networkLEDBlinkDriver_remove,
};
/*		*/ 		

// init function

/* __init network_LED_driver_init(void)
ADD THIS FOR HW3 */ 
static int __init char_driver_init(void){
	printk(KERN_INFO "Module loading... \n");	
 

	if(alloc_chrdev_region(&mydev.mydev_node, 0, DEVCNT, DEVNAME)){
		printk(KERN_ERR "Allocating chrdev failed \n");
		return -1;
	}

	printk(KERN_INFO "Allocated %d devices at major: %d\n", DEVCNT, MAJOR(mydev.mydev_node));

	/*initialize the character device */
	cdev_init(&mydev.cdev, &mydev_fops);
	mydev.cdev.owner = THIS_MODULE;

	if(cdev_add(&mydev.cdev, mydev.mydev_node, DEVCNT)){
		printk(KERN_ERR "cdev_add failed \n");
		/* clean up allocation of chrdev */
		unregister_chrdev_region(mydev.mydev_node, DEVCNT);
		return -1;
	}

	//PCI
	if(pci_register_driver(&pci_networkLEDBlinkDriver)){
			printk(KERN_ERR "pci_register_driver failed \n");
		//	goto unregister_pci_driver;
	}
	

	return 0;
}

// exit function 
/* __exit network_LED_driver_exit(void) 
ADD THIS FOR HW3*/
static void __exit char_driver_exit(void){
	cdev_del(&mydev.cdev);
	unregister_chrdev_region(mydev.mydev_node, DEVCNT);
	pci_unregister_driver(&pci_networkLEDBlinkDriver);
	printk(KERN_INFO "Unloaded module \n");
}
//MODULE_TABLE(pci,pci_networkLEDBlinkDriver_table)
MODULE_AUTHOR("Abdullah Barghouti");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.2");
module_init(char_driver_init);
module_exit(char_driver_exit);

