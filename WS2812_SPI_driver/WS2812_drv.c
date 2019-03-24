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


#define SPIDEV_MAJOR	201	
#define SPIDEV_MINOR	61	
#define SPI_DEVICE_NAME "WS2812"
#define SPI_CLASS_NAME "WS2812"
#define RESET 459 //IOCTL

#define ZERO 0xC0
#define ONE 0xF8

#define LOW ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO, ZERO
#define HIGH ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE

#define GREEN HIGH, LOW, LOW
#define YELLOW HIGH, HIGH, LOW
#define RED LOW, HIGH, LOW
#define MAGENTA LOW, HIGH, HIGH
#define BLUE LOW, LOW, HIGH
#define CYAN HIGH, HIGH, HIGH
#define WHITE HIGH, HIGH, HIGH
#define NONE LOW, LOW, LOW


struct spidev_data {
	dev_t			devt;
	struct spi_device	*spi;
	u32			speed_hz;
	
	
}*spidev;

uint8_t tx[] = {
	BLUE, YELLOW, RED, MAGENTA, BLUE, CYAN, GREEN,
	YELLOW, RED, MAGENTA, BLUE, CYAN, GREEN,
	YELLOW, RED, MAGENTA, 
};


static struct class *spidev_class;

static void spidev_complete(void *arg)
{
	complete(arg);
}

//Send the message buffer
static int send_message(uint8_t *new_tx){
	int status;
	struct spi_message message;
	struct spi_transfer t = {
		.tx_buf = new_tx,
		.len = sizeof(tx),
		.speed_hz = spidev->speed_hz,	
	};
	        
	spi_message_init(&message);
	spi_message_add_tail(&t, &message);

	DECLARE_COMPLETION_ONSTACK(done);

	message.complete = spidev_complete;
	message.context = &done;

	status = spi_async(spidev->spi, &message);

	if (status == 0) {
		wait_for_completion(&done);
		status = message.status;
		if (status == 0)
			status = message.actual_length;
	}
	return status;
}

//gets the number of leds to be lit from the user and lights them up
static ssize_t spidev_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	int status;
	unsigned i, j, k=0;
	int n;
	uint8_t new_tx[8*3*16];
        
	copy_from_user(&n, (int *)buf, sizeof(int));
	
	printk("Number of pixels %d\n", n);
	for(k =0; k<80;k++){
		j = k %16;
		for(i = 0; i <16*8*3; i++)
		{	
			if(i<(n+j)*8*3 && i>= j*24)
				new_tx[i] = tx[i];
	
			//set the remaining values to zero
			else
				new_tx[i] = ZERO;
		}
		mdelay(50);
		status = send_message(new_tx);
		if(status == 0)
			return status;
	}
	
	return status;
}

//To reset the pins and reset the led ring
static long spidev_ioctl(struct file *file, unsigned int ioctl_num, unsigned long arg)
{
	int status;
	uint8_t reset[] = {NONE, NONE, NONE, NONE, 
			  NONE, NONE, NONE, NONE,
			  NONE, NONE, NONE, NONE,
			  NONE, NONE, NONE, NONE};

	switch(ioctl_num)
	{
		case RESET: 	
			printk("Resetting IO11 pin to be used as SPI_MOSI to data_in!\n");
			gpio_direction_output(24, 0);
			gpio_direction_output(44, 1);
			spidev->spi->mode = SPI_MODE_3;
			status = send_message(reset);
			return status;
		default:
			printk("Ioctl command not found!\n");
			return -1;
	}

} 

static int spidev_open(struct inode *inode, struct file *filp)
{	
	
	filp->private_data = spidev;
	nonseekable_open(inode, filp);
	return 0;
}

static int spidev_release(struct inode *inode, struct file *filp)
{

	filp->private_data = NULL;
	return 0;

}

//file operations
static const struct file_operations spidev_fops = {
	.owner 	=	THIS_MODULE,
	.write 	=	spidev_write,
	.unlocked_ioctl = spidev_ioctl,
	.open 	=	spidev_open,
	.release=	spidev_release,
};

//finding the device match with no vendor part
static const struct spi_device_id spidev_dt_ids[] = {
    { "WS2812", 0 },
    { }
};

MODULE_DEVICE_TABLE(spi, spidev_dt_ids);


//when WS2812 is inserted
static int spidev_probe(struct spi_device *spi)
{
	int			status;
	struct device 		*dev;
        printk("Found device\n");

	spidev = kzalloc(sizeof(*spidev), GFP_KERNEL);
	if (!spidev)
		return -ENOMEM;
	
	spidev->spi = spi;

	spidev->speed_hz = spi->max_speed_hz;

	spidev->devt = MKDEV(SPIDEV_MAJOR, SPIDEV_MINOR);

	dev = device_create(spidev_class, &spi->dev, spidev->devt,
			    spidev, SPI_DEVICE_NAME);

	status = PTR_ERR_OR_ZERO(dev);

	if (status == 0)
		spi_set_drvdata(spi, spidev);
	else	{
		printk("Error creating the device!\n");
		kfree(spidev);
	}
	return status;
}

//remove the device
static int spidev_remove(struct spi_device *spi)
{
	spidev->spi = NULL;
	device_destroy(spidev_class, spidev->devt);
	kfree(spidev);
	printk("Device Removed!\n");	
	return 0;
}


static struct spi_driver spidev_spi_driver = {
	.driver = {
		.name =		"WS2812_driver",
		.owner =	THIS_MODULE,
		.of_match_table = of_match_ptr(spidev_dt_ids),
	},
	.probe =	spidev_probe,
	.remove =	spidev_remove,

};


static int __init spidev_init(void)
{
	int status;

	status = register_chrdev(SPIDEV_MAJOR, "spi", &spidev_fops);
	if (status < 0)
		return status;

	spidev_class = class_create(THIS_MODULE, SPI_CLASS_NAME);
	if (IS_ERR(spidev_class)) {
		unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);
		return PTR_ERR(spidev_class);
	}

	status = spi_register_driver(&spidev_spi_driver);
	if (status < 0) {
		class_destroy(spidev_class);
		unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);
	}	

	//configure IO11 pin to be spi MOSI
	gpio_request(24, "sysfs");   
	gpio_request(44, "sysfs");  
	gpio_export(24, true);  	 
	gpio_export(44, true);	
	return status;
}


module_init(spidev_init);

static void __exit spidev_exit(void)
{
	spi_unregister_driver(&spidev_spi_driver);
	class_destroy(spidev_class);
	unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);
	//free gpio pins
	gpio_free(24);
	gpio_free(44);
}
module_exit(spidev_exit);

MODULE_DESCRIPTION("SPI device driver for Neopixel ring integrated by WS2812");
MODULE_LICENSE("GPL");
