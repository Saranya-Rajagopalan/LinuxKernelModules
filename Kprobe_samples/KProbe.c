#include<linux/module.h>
#include<linux/version.h>
#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/kprobes.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/moduleparam.h>
#include <linux/rbtree.h>
#include <linux/kallsyms.h>
#include<linux/ptrace.h>
#include <linux/sched.h>
#include<asm/current.h>
#include<asm/msr.h>


#define DEVICE_NAME "RBProbe"
#define READ_FUNCTION "read"
#define WRITE_FUNCTION "write"

int kprobe_open(struct inode *, struct file *);
int kprobe_close(struct inode *, struct file *);
static ssize_t kprobe_write(struct file *, const char *, size_t, loff_t *);
static ssize_t kprobe_read(struct file *, char *, size_t, loff_t *);
static dev_t kprobe_dev_number;
static struct device *kprobe_device;
struct class *kprobe_class;
static struct kprobe kp;

/* File operations structure. Defined in linux/fs.h */
static struct file_operations kprobe_fops = {
    .owner		= THIS_MODULE,           /* Owner */
    .open		= kprobe_open,        /* Open method */
    .release		= kprobe_close,     /* Release method */
    .write		= kprobe_write,       /* Write method */
    .read		= kprobe_read,        /* Read method */
};

//Character device structure
struct kprobe_struct {
	struct cdev cdev;            		//Character device structure  
	//struct kprobe kp;
	//char name[20];                		//Name of the device
} *kprobep;



struct user_input{
   unsigned int offset;
   int flag;
   int function_type;
};

struct trace_data{
      long int pid;
      unsigned long int time_stamp;
      void *address;
}rd;

int Pre_Handler(struct kprobe *p, struct pt_regs *regs){
    printk("Pre_Handler :\n");
    return 0;
}
 
void Post_Handler(struct kprobe *p, struct pt_regs *regs, unsigned long flags){
    rd.address = p->addr;
    rd.pid = current->pid;
    rd.time_stamp = 0;
    printk(KERN_INFO "Post_Handler: p->addr = 0x%p, ip = %lx,"
			" flags = 0x%lx\n", p->addr, regs->ip, regs->flags);
    printk("Post_handler: pid = %ld timestamp = %lu, Address =0x%p \n", rd.pid , rd.time_stamp, rd.address);
    
}

int __init kprobe_init(void)
{
    	int ret;
	// Request dynamic allocation of a device major number
	if (alloc_chrdev_region(&kprobe_dev_number, 0, 1, "KPROBE") < 0)
		{
			printk(KERN_DEBUG "Can't register kprobe\n"); 
			return -1;
		}
	//Class creation
	kprobe_class = class_create(THIS_MODULE, DEVICE_NAME);
	// Allocate memory for the per-device structure
	kprobep = kmalloc(sizeof(struct kprobe_struct), GFP_KERNEL);

	if (!kprobep) {
		printk("Bad Kmalloc\n"); return -ENOMEM;
	} 

	// Connect the file operations with the cdev
	cdev_init(&kprobep->cdev, &kprobe_fops);
	//kprobep->cdev.owner = THIS_MODULE;

	// Connect the major/minor number to the cdev
	ret = cdev_add(&kprobep->cdev, (kprobe_dev_number), 1);

	if (ret) {
		printk("Bad cdev\n");
		return ret;
	}
	
	/* Send uevents to udev, so it'll create /dev nodes */
	kprobe_device = device_create(kprobe_class, NULL, MKDEV(MAJOR(kprobe_dev_number), 0), NULL, DEVICE_NAME);

	return 0;

}
 
void __exit kprobe_exit(void)
{
    	unregister_kprobe(&kp);
    	printk("Kprobe module removed\n ");
}	

int kprobe_open(struct inode *inode, struct file *fp)
{
	struct kprobe_struct *kprobep;
	printk("Opening RBprobe\n");

	// Get the per-device structure that contains this cdev 
	kprobep = container_of(inode->i_cdev, struct kprobe_struct, cdev);

	// Easy access to cmos_devp from rest of the entry points 
	fp->private_data = kprobep;
	printk("\n%s is opening! \n", DEVICE_NAME);
	return 0;
}

int kprobe_close(struct inode *inode, struct file *fp){
	printk("Closing RBProbe\n");
	return 0;
}


ssize_t kprobe_write(struct file *fp, const char *buf, size_t count, loff_t *ppos){
	//struct kprobe_struct *kprobep = fp->private_data;
	struct user_input *temp = kmalloc(sizeof(struct user_input), GFP_KERNEL);
	printk("Write Method: Entering Kprobe write\n"); 
	copy_from_user(temp, (struct user_input*) buf, sizeof(struct user_input));
	printk("Flag %d, Function_type %d Offset %d", temp->flag, temp->function_type, temp->offset);
	kp.pre_handler = Pre_Handler;
	kp.post_handler = Post_Handler;
	if(temp->function_type ==0){
		if(temp->flag==0)
			unregister_kprobe(&kp);
		else if(temp->flag==1){
			printk("Kprobe attaching to read");
			kp.addr = (kprobe_opcode_t *)(kallsyms_lookup_name("rbtdevice_driver_read")+temp->offset);	
			register_kprobe(&kp);}
	}
	else if (temp->function_type == 1)
	{	if(temp->flag==0)
			unregister_kprobe(&kp);
		else if(temp->flag==1){
			printk("Kprobe attaching to write");
			kp.addr = (kprobe_opcode_t *)(kallsyms_lookup_name("rbtdevice_driver_write")+temp->offset);	
			register_kprobe(&kp);}
	}
	else printk("Enter only 0 or 1");	
	return count; 
}


ssize_t kprobe_read(struct file *fp, char *buf, size_t count, loff_t *ppos){
	int bytes_read = sizeof(struct trace_data);
	copy_to_user(buf, &rd, sizeof(struct trace_data));
	return bytes_read; 
}


module_init(kprobe_init);
module_exit(kprobe_exit);
MODULE_AUTHOR("Team25");
MODULE_DESCRIPTION("KPROBE MODULE");
MODULE_LICENSE("GPL");

