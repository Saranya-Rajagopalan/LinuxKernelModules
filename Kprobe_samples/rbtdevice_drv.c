/* ----------------------------------------------- DRIVER rbtdevice --------------------------------------------------
 
 Device driver which uses Red-Black Tree data structure. 
 Note: does not support concurrent operations.
 
 ----------------------------------------------------------------------------------------------------------------*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/rbtree.h>


#define DEVICE_NAME	"rbtdevice"  // Name of the device
#define IOCTL_READ_FROM_MAXIMUM 899	//IOCTL Number for reading from the end or beginning

//Datatype of the data that's stored in each node
typedef struct rb_object{
int key;
int data;
}rb_object_t;

//RB node with data
struct rbtdevice_node{
     rb_object_t rbt_object;
     struct rb_node rbt_node;
};


//Character device structure
struct rbtdevice_dev {
	struct cdev cdev;            		//Character device structure  
	struct rb_root root;	      		//Root of the RB tree
	char name[20];                		//Name of the device
	int read_from_maximum;	      	//flag that is used to read from maximum or minimum
} *rbtdevice_devp;

static dev_t rbtdevice_dev_number;       // Device number
struct class *rbtdevice_dev_class;       // Tie with the device model
static struct device *rbtdevice_dev_device;

static char *user_name = "SaranyaRajagopalan";


// search RB tree for a node
// Takes the root of the tree and the key to be searched for
// And returns the Node if the key is found
struct rbtdevice_node *rb_search(struct rb_root *root, int key)
    {
	printk("Entering Search method...\n");
        struct rb_node *node = root->rb_node; 

        while (node)
	{
	    struct rbtdevice_node *data_node = rb_entry(node, struct rbtdevice_node, rbt_node); //get the containing struct
	    if (data_node->rbt_object.key > key)
		node = node->rb_left;
	    else if (data_node->rbt_object.key < key)
		node = node->rb_right;
	    else{
		printk("FOUND!");
		return data_node;  
		}
  	}
	return NULL;
    }

// returns the rbtdevice_node containing the least key value
struct rbtdevice_node *find_minimum(struct rb_root *root)
    {
	printk("Entering find minimum method...\n");
        struct rb_node *node = root->rb_node; 
	
	struct rbtdevice_node *parent;

        while (node)
	{
	    parent = rb_entry(node, struct rbtdevice_node, rbt_node);
	    node = node->rb_left;
  	}
	printk("Minimum value is %d",parent->rbt_object.key);
	return parent;
    }

// returns the rbtdevice_node containing the greatest key value
struct rbtdevice_node *find_maximum(struct rb_root *root)
    {
	printk("Entering find maximum method...\n");
        struct rb_node *node = root->rb_node; 
	struct rbtdevice_node *parent;
        while (node)
	{
	        parent = rb_entry(node, struct rbtdevice_node, rbt_node);
		node = node->rb_right;
  	}
	printk("Maximum value is %d",parent->rbt_object.key);
	return parent;
    }


//Insertion of a node
//Takes the root of the rbtree and the new node to be inserted
void rb_insert(struct rb_root *root, struct rbtdevice_node *new)
    {	
	//printk("Inserting node...");
        struct rb_node **link = &root->rb_node, *parent;
	int key = new->rbt_object.key;

	while (*link)
	{
	    parent = *link;
	    struct rbtdevice_node *data_node = rb_entry(parent, struct rbtdevice_node, rbt_node);

	    if (data_node->rbt_object.key > key){
		printk("key value is %d, data is %d. Moving left.", data_node->rbt_object.key,data_node->rbt_object.data);
		link = &(*link)->rb_left;
		}
	    else{
		printk("key value is %d, data is %d. Moving right.", data_node->rbt_object.key, data_node->rbt_object.data);
		link = &(*link)->rb_right;
		}
	}
	new->rbt_node.rb_left = NULL;
	new->rbt_node.rb_right = NULL;
	rb_link_node(&new->rbt_node, parent, link);
	rb_insert_color(&new->rbt_node, root);
    }

/*
* Open rbtdevice driver
*/
int rbtdevice_driver_open(struct inode *inode, struct file *file)
{
	struct rbtdevice_dev *rbtdevice_devp;
	printk("Opening\n");

	// Get the per-device structure that contains this cdev 
	rbtdevice_devp = container_of(inode->i_cdev, struct rbtdevice_dev, cdev);

	// Easy access to cmos_devp from rest of the entry points 
	file->private_data = rbtdevice_devp;
	printk("\n%s is opening! \n", rbtdevice_devp->name);
	return 0;
}

//Release RBTdevice
int rbtdevice_driver_release(struct inode *inode, struct file *file)
{
	struct rbtdevice_dev *rbtdevice_devp = file->private_data;
	printk("\n%s is closing!\n", rbtdevice_devp->name);
	return 0;
}

//Write to rbtdevice driver
ssize_t rbtdevice_driver_write(struct file *file, const char *buf,
           size_t count, loff_t *ppos)
{
        printk("Kernel write called!\n");
	struct rbtdevice_dev *rbtdevice_devp = file->private_data;
        struct rbtdevice_node * temp= NULL;

        temp = kmalloc(sizeof(struct rbtdevice_node),GFP_KERNEL);
        copy_from_user(&temp->rbt_object, buf, sizeof(rb_object_t));

	printk("Writing %d %d!",temp->rbt_object.key, temp->rbt_object.data);
	
	struct rbtdevice_node *search_node = rb_search(&rbtdevice_devp->root, temp->rbt_object.key);
	if(search_node ==NULL){
		if(temp->rbt_object.data!=0)
			rb_insert(&rbtdevice_devp->root, temp);}
	else if((temp->rbt_object.data) ==0){
		printk("Data field is zero. Deleting the node\n");
		rb_erase(&search_node->rbt_node, &rbtdevice_devp->root);
		kfree(search_node);
	}
	else{
		printk("Changing the data value\n");
		search_node->rbt_object.data = temp->rbt_object.data;
	}
	return 0;
}


//Read from the device driver
ssize_t rbtdevice_driver_read(struct file *file, char *buf,
           size_t count, loff_t *ppos)
{
	int bytes_read= 0;

	struct rbtdevice_dev *rbtdevice_devp = file->private_data;
	struct rbtdevice_node *node_to_be_read;
	if (rbtdevice_devp->read_from_maximum ==0)	
		node_to_be_read = find_minimum(&rbtdevice_devp->root);
	else 
		node_to_be_read = find_maximum(&rbtdevice_devp->root);
	
	if(node_to_be_read->rbt_object.key ==NULL){
		printk("Attempt to read from an empty tree. Quitting! \n");
		return -1;
	}

	printk("RBTdevice Read key, data pair is %d %d", node_to_be_read->rbt_object.key, node_to_be_read->rbt_object.data);

	copy_to_user(buf, &node_to_be_read->rbt_object, sizeof(rb_object_t));
	
	bytes_read = sizeof(rb_object_t);

	rb_erase(&node_to_be_read->rbt_node, &rbtdevice_devp->root);

	kfree(node_to_be_read);
	printk("Finished reading %d bytes!\n", bytes_read);
	return bytes_read;
}



// ioctl
// if ioctl IOCTL_READ_FROM_MAXIMUM is called with argument 1, the rbtree is read from the end
// if it's set to 0, the rbtree is read from the beginning
long rbtdevice_driver_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_value)
{
	struct rbtdevice_dev *rbtdevice_devp = file->private_data;
	if (ioctl_num == IOCTL_READ_FROM_MAXIMUM){
		switch(ioctl_value){
		case 1: printk("IOCTL value is 1. Setting read_from_max flag to 1\n");
			rbtdevice_devp->read_from_maximum = 1;
			break;
		case 0: printk("IOCTL value is 1. Setting read_from_max flag to 1\n");
			rbtdevice_devp->read_from_maximum = 0;
			break;
		default:printk("Invalid Ioctl value. Set it to 0 or 1!\n");
		}	
	}

}

/* File operations structure. Defined in linux/fs.h */
static struct file_operations rbtdevice_fops = {
    .owner		= THIS_MODULE,           /* Owner */
    .open		= rbtdevice_driver_open,        /* Open method */
    .release	= rbtdevice_driver_release,     /* Release method */
    .write		= rbtdevice_driver_write,       /* Write method */
    .read		= rbtdevice_driver_read,        /* Read method */
     .unlocked_ioctl    = rbtdevice_driver_ioctl,
};



// Driver Initialization
 
int __init rbtdevice_driver_init(void)
{
	int ret;

	// Request dynamic allocation of a device major number
	if (alloc_chrdev_region(&rbtdevice_dev_number, 0, 1, DEVICE_NAME) < 0)
		{
			printk(KERN_DEBUG "Can't register device\n"); 
			return -1;
		}

	//Class creation
	rbtdevice_dev_class = class_create(THIS_MODULE, DEVICE_NAME);

	// Allocate memory for the per-device structure
	rbtdevice_devp = kmalloc(sizeof(struct rbtdevice_dev), GFP_KERNEL);
		
	if (!rbtdevice_devp) {
		printk("Bad Kmalloc\n"); return -ENOMEM;
	}

	// Request I/O region
	sprintf(rbtdevice_devp->name, DEVICE_NAME);

	// Connect the file operations with the cdev
	cdev_init(&rbtdevice_devp->cdev, &rbtdevice_fops);
	rbtdevice_devp->cdev.owner = THIS_MODULE;
	rbtdevice_devp->read_from_maximum = 0;

	// Connect the major/minor number to the cdev
	ret = cdev_add(&rbtdevice_devp->cdev, (rbtdevice_dev_number), 1);

	if (ret) {
		printk("Bad cdev\n");
		return ret;
	}
	
	//Point the device pointer to the root of the RBT
        rbtdevice_devp->root  = RB_ROOT;

	/* Send uevents to udev, so it'll create /dev nodes */
	rbtdevice_dev_device = device_create(rbtdevice_dev_class, NULL, MKDEV(MAJOR(rbtdevice_dev_number), 0), NULL, DEVICE_NAME);		

	printk("Rbtdevice driver initialized.\n");
	return 0;
}

void inorder_deletion(struct rb_node *root)
{   
    if ( root!= NULL)
    {   struct rbtdevice_node *containerstruct = rb_entry(root, struct rbtdevice_node, rbt_node); //get the containing struct
	inorder_deletion(root->rb_left);
	rb_erase(&containerstruct->rbt_node, &rbtdevice_devp->root);
	kfree(containerstruct);
	inorder_deletion(root->rb_right);
    }
}

// Exitting
void __exit rbtdevice_driver_exit(void)
{	
	struct rb_node *node =  (rbtdevice_devp->root).rb_node; 

	printk("Deleting all the nodes\n");  //Deleting all the nodes in order

	inorder_deletion(node);
	// unregister device
	unregister_chrdev_region((rbtdevice_dev_number), 1);

	//destroy device
	device_destroy (rbtdevice_dev_class, MKDEV(MAJOR(rbtdevice_dev_number), 0));
	cdev_del(&rbtdevice_devp->cdev);
	kfree(rbtdevice_devp);	
	class_destroy(rbtdevice_dev_class);
	printk("Rbtdevice driver removed.\n");
}

module_init(rbtdevice_driver_init);
module_exit(rbtdevice_driver_exit);
MODULE_LICENSE("GPL v2");
