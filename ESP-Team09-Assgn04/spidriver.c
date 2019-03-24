#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/kthread.h>

#define N_SPI_MINORS 1
#define SPIDEV_MAJOR 438
#define SPI_DEVICE_NAME "MAX7219"
#define SPI_CLASS_NAME "MAX7219_class"

#define GET_PATTERNS 153


#define DECODE 0x09
#define INTENSITY 0x0A
#define SCAN_LIMIT 0x0B
#define SHUTDOWN 0x0C
#define DISPLAY_TEST 0x0F

#define MY_MAGIC 'G'
#define DISPLAY_PATTERN _IO(MY_MAGIC,1)
#define PASS_STRUCT_ARRAY_SIZE _IOW(MY_MAGIC,2,int)

static struct class *spidev_class;


//function prototypes
//modules
int spidev_init(void);
void spidev_exit(void);
//fops
static int spidev_open(struct inode *, struct file *);
static int spidev_release(struct inode *, struct file *);
static ssize_t spidev_write(struct file *, const char *, size_t, loff_t *);
static long spidev_ioctl(struct file *, unsigned int, unsigned long);
//spidevice drivers
static int spidev_probe(struct spi_device *);
static int spidev_remove(struct spi_device *);
//helper functions
static void spidev_complete(void *);
static int send_message(uint8_t *);
static int spi_display_setup(void);
void configure_io_pins(void);
int display_thread(void *);

//master side proxy for an SPI slave device
struct spidev_data{
	dev_t devt;
	struct spi_device *spi;
	char name[10];
	int speed_hz;
}*spidev;

struct data{
	int pattern;
	int duration;
};

typedef struct{
	char led[8];
}PATTERN;

PATTERN pattern_array[10];

int SIZE =10;
struct data sequence[10];
char *thread_name = "Display_thread";

//----------------------------------------------------------------------------------------------
//Send the message buffer

static void spidev_complete(void *arg){
	complete(arg);
}


static int send_message(uint8_t* pattern){
	int status;
	DECLARE_COMPLETION_ONSTACK(done);
	struct spi_message message;
	struct spi_transfer t= {
			.tx_buf = pattern,
			.len = 2,
			.speed_hz = spidev->speed_hz,	
			.cs_change = 1,
			.delay_usecs =1,
			.rx_buf = 0,
			.bits_per_word = 8
		};
	spi_message_init(&message);
	spi_message_add_tail(&t, &message);

	message.complete = spidev_complete;
	message.context = &done;

	gpio_direction_output(15, 0);
	status = spi_async(spidev->spi, &message);

	if (status == 0) {
		wait_for_completion(&done);
		status = message.status;
		if (status == 0)
			status = message.actual_length;
	}
	gpio_direction_output(15, 1);
	
	return status;
}

static int spi_display_setup(void){
	int status =0;
	uint8_t pattern[2];
	
	pattern[0] = DECODE;
	pattern[1] = 0x00;
	status = send_message(pattern);
	mdelay(1);
	if(status == 0)
		return status;

	pattern[0] = DISPLAY_TEST;
	pattern[1] = 0x00;
	status = send_message(pattern);
	mdelay(1);
	if(status == 0)
		return status;

	pattern[0] = SHUTDOWN;
	pattern[1] = 0x01;
	status = send_message(pattern);
	mdelay(1);
	if(status == 0)
		return status;

	pattern[0] = INTENSITY;
	pattern[1] = 0x0F;
	status = send_message(pattern);
	mdelay(1);
	if(status == 0)
		return status;

	pattern[0] = SCAN_LIMIT;
	pattern[1] = 0x07;
	status = send_message(pattern);
	mdelay(1);
	if(status == 0)
		return status;
	return status;
}

int display_thread(void *stuff){
	int i;
	uint8_t pattern[2];
	int position=0;
	struct data to_display;
	for(i=0; i<10;i++){
		to_display = sequence[i];
		if(to_display.pattern ==0 && to_display.duration ==0){
			pattern[0] = 0x0C;
			pattern[1] = 0x00;
			send_message(pattern);
			return 0;
		}
		
		spi_display_setup();
		printk("Displaying pattern %d for %d ms\n", to_display.pattern, to_display.duration);
		for(position =0; position<8; position++){
			pattern[0] = position+1;
			pattern[1] = pattern_array[to_display.pattern].led[position];
			send_message(pattern);
			mdelay(1);
			}
		mdelay(to_display.duration);
	}
	return 0;
}

//gets the number of leds to be lit from the user and lights them up
static ssize_t spidev_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{	
	ssize_t status;
	int *random_pointer;
	random_pointer = (int *)kzalloc(sizeof(int), GFP_KERNEL);
	
	status = copy_from_user((struct data *)&sequence, (int * __user)buf, sizeof(sequence));
	kthread_run(&display_thread,(void *)random_pointer, "%s-#%d", thread_name, 0);
	return status;
}

//To reset the pins and reset the led ring
static long spidev_ioctl(struct file *file, unsigned int ioctl_num, unsigned long arg)
{
	int ret =0;

	switch(ioctl_num){

		case DISPLAY_PATTERN:
			ret = copy_from_user(&pattern_array, (PATTERN *)arg, sizeof(pattern_array));
			printk("Received 10 display patterns\n");
			break;

		case PASS_STRUCT_ARRAY_SIZE:
		    	printk("Inside PASS_STRUCT_ARRAY_SIZE\n");
			SIZE =arg;
			if(ret < 0){
			    printk("Error in PASS_STRUCT_ARRAY_SIZE\n");
			    return -1;  
			}
			printk("Struct Array Size : %d\n",SIZE);
			break;

		default :
			return -ENOTTY;
	}

	return 0;

}


static int spidev_open(struct inode *inode, struct file *filp)
{	
	configure_io_pins();
	filp->private_data = spidev;
	nonseekable_open(inode, filp);
	return 0;
}

static int spidev_release(struct inode *inode, struct file *filp)
{
	gpio_free(5);
	gpio_free(15);
	gpio_free(7);
	gpio_free(24);
	gpio_free(42);
	gpio_free(30);
	gpio_free(25);
	gpio_free(43);
	gpio_free(31);
	gpio_free(44);
	gpio_free(46);
	filp->private_data = NULL;
	return 0;

}


static const struct file_operations spidev_fops = {
	.owner 		= THIS_MODULE,
	.write 		= spidev_write,
	.unlocked_ioctl = spidev_ioctl,
	.open		= spidev_open,
	.release	= spidev_release,

};



//-----------------------------------------------------------------------------------------------
//finding the device match with no vendor part
static const struct spi_device_id spidev_dt_ids[] = {
    { "MAX7219", 0 },
    { },
};


MODULE_DEVICE_TABLE(spi, spidev_dt_ids);


static int spidev_probe(struct spi_device *spi){
	struct device *dev;
	int status;
	printk("Found device!\n");

	spidev = kzalloc(sizeof(*spidev), GFP_KERNEL);
	if(!spidev)
		return -ENOMEM;

	spidev->spi = spi;

	spidev->speed_hz = spi->max_speed_hz;
	spidev->devt = MKDEV(SPIDEV_MAJOR, 1);

	dev = device_create(spidev_class, &spi->dev, spidev->devt, spidev, SPI_DEVICE_NAME);
	
	status = PTR_ERR_OR_ZERO(dev);

	if(status==0)
		spi_set_drvdata(spi, spidev);
	else	{
		printk("Error creating the device!\n");
		kfree(spidev);
	}
	
	spidev->devt = (dev_t)dev;
	return status;
}

static int spidev_remove(struct spi_device *spi){
	spidev = spi_get_drvdata(spi);
	spidev->spi = NULL;
	device_destroy(spidev_class, spidev->devt);
	kfree(spidev);
	printk("Device Removed!\n");
	return 0;

}

void configure_io_pins(void){
	gpio_request(5, "sysfs");   
	gpio_request(15, "sysfs");  
	gpio_request(7, "sysfs");   
	gpio_request(42, "sysfs");  
	gpio_request(24, "sysfs");   
	gpio_request(30, "sysfs");  
	gpio_request(25, "sysfs");   
	gpio_request(43, "sysfs");  
	gpio_request(31, "sysfs");   
	gpio_request(44, "sysfs");  
	gpio_request(46, "sysfs");   
	gpio_export(5, "true");
	gpio_export(15, "true");
	gpio_export(7, "true");
	gpio_export(24, "true");
	gpio_export(42, "true");
	gpio_export(30, "true");
	gpio_export(25, "true");
	gpio_export(43, "true");
	gpio_export(31, "true");
	gpio_export(44, "true");
	gpio_export(46, "true");
	gpio_direction_output(5, 1);
	gpio_direction_output(15, 0);
	gpio_direction_output(7, 0);
	gpio_direction_output(24, 0);
	gpio_direction_output(42, 0);
	gpio_direction_output(30, 0);
	gpio_direction_output(25, 0);
	gpio_direction_output(43, 0);
	gpio_direction_output(31, 1);
	gpio_direction_output(44, 1);
	gpio_direction_output(46, 1);
}


//host side protocol driver
struct spi_driver spidev_spi_driver = {
	.driver = {
		.name =		"MAX7219_driver",
		.owner =	THIS_MODULE,
		.of_match_table = of_match_ptr(spidev_dt_ids),
	},
	.probe = spidev_probe,
	.remove = spidev_remove,
};

//------------------------------------------------------------------------------------------

int __init spidev_init(void)
{
	//request dynamic allocation of major numner, first minor number and number of devices
	//DEVICE_NAME appears under /proc/devices
	int status;

	status = register_chrdev(SPIDEV_MAJOR, "spi", &spidev_fops);
	if (status < 0)
		return status;

	
	spidev_class = class_create(THIS_MODULE, SPI_CLASS_NAME);		//Creates an entry under /dev

	if(IS_ERR(spidev_class)){
                unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);
                printk("Could not add cdev structure to kernel\n");
                return -1;
	}

	status = spi_register_driver(&spidev_spi_driver);
        if(status<0){
                unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);
                kfree(spidev);
                class_destroy(spidev_class);
                printk("Could not add cdev structure to kernel\n");
                return -1;
        }


	printk("Initialized driver module!\n");

	return status;
}


void spidev_exit(void){
	spi_unregister_driver(&spidev_spi_driver);
	class_destroy(spidev_class);

	unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);

	kfree(spidev);
	printk("Unregistered device\n");
	return;
}


module_init(spidev_init);
module_exit(spidev_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Saranya Rajagopalan, srajag25@asu.edu");
