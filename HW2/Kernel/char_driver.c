/*
Abdullah Barghouti 
ECE 373 HW 2
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

#define DEVCNT 5
#define DEVNAME "char_driver"

static struct mydev_dev{
	struct cdev cdev;
	dev_t mydev_node;
	int sys_int;
	int syscall_val;
} mydev;


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
// open, read, write struct 
static struct file_operations mydev_fops = {
	.owner = THIS_MODULE,
	.open = char_driver_open,
	.read = char_driver_read,
	.write = char_driver_write,
};	

// init function
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
	 return 0;	 
}

// exit function 
static void __exit char_driver_exit(void){
	cdev_del(&mydev.cdev);
	unregister_chrdev_region(mydev.mydev_node, DEVCNT);	
	printk(KERN_INFO "Unloaded module \n");
}

MODULE_AUTHOR("Abdullah Barghouti");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.2");
module_init(char_driver_init);
module_exit(char_driver_exit);
