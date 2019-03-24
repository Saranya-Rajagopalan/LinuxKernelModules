#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>	//container_of defined in kernel.h
#include <linux/slab.h>		//memory management (kmalloc and kfree)
#include <linux/device.h>	//class_create, device_create for creating /dev nodes
#include <linux/kdev_t.h>	//MAJOR, MINOR, MKNOD
#include <linux/fs.h>		//file_operations struct
#include <linux/cdev.h>		//struct cdev used to represent character device is from this header
#include <asm/uaccess.h>	//copy_to_user and copy_from_user
#include <linux/gpio.h>		//gpio functions
#include <linux/hrtimer.h>	//for timing the leds
#include <linux/ktime.h>	//ktime_t defined in this

#define DEVICE_NAME "RGBLed"
#define DEVICE_COUNT 1

#define NaN 100000
#define CYCLE_DURATION 20000 //in microseconds

#define CONFIG 899

//Index for pin multiplexing
#define	LINUX_PIN 0
#define DIRECTION_PIN 1
#define	PULL_UP 2
#define	PIN_MUX 3
#define	VALUE 4
#define	BOTH_INTERRUPT_SUPPORT 5

//function prototypes. 
int RGBLed_init (void);
void RGBLed_exit (void);
int RGBLed_open (struct inode *, struct file *);
int RGBLed_close (struct inode *, struct file *);
ssize_t RGBLed_read (struct file *, char *, size_t, loff_t *);
ssize_t RGBLed_write (struct file *, const char *, size_t, loff_t *);
long RGBLed_ioctl (struct file *, unsigned int, unsigned long);
void enable_display(bool);

//pin mux table for configuring gpio (not for any other functions)
static int pin_mux[20][6] ={{11, 32, 33, NaN, NaN,0},			//IO0
			 {12, 28, 29, 45, 0, 0},			//IO1
			 {13, 34, 35, 77, 0, 0},			//IO2
			 {62, 16, 17, 76, 0, 1},			//IO3
			 {6, 36, 37, NaN, NaN, 1},			//IO4
			 {0, 18, 19, 66, 0, 1},				//IO5
			 {1, 20, 21, 68, 0, 1},				//IO6
			 {38, NaN, 39, NaN, NaN, 0},			//IO7
			 {40, NaN, 41, NaN, NaN, 0},			//IO8
			 {4, 22, 23, 70, 0, 1},				//IO9
			 {10, 26, 27, 74, 0, 0},			//IO10
			 {5, 24, 25, 44, 0, 1},				//IO11
			 {15, 42, 43, NaN, NaN, 0},			//IO12
			 {7, 30, 31, 46, 0, 1},				//IO13
			 {48, NaN, 49, NaN, NaN, 1},			//IO14
			 {50, NaN, 51, NaN, NaN, 1},			//IO15
			 {52, NaN, 53, NaN, NaN, 1},			//IO16
			 {54, NaN, 55, NaN, NaN, 1},			//IO17
			 {56, NaN, 57, 60, 1, 1},			//IO18
			 {58, NaN, 59, 60, 1, 1}};			//IO19




static struct hrtimer timer;
bool exit = false;
bool on_time;
static struct device *RGBLed_char_device;
static struct class *RGBLed_class;
static int major = 0;


//each open file is associated with it's own set of file operations in charge of syscalls
//file: object. fops: methods (in oops terminology) 
static struct file_operations RGBLed_fops = {
	.owner = THIS_MODULE,		//prevents the module to be unloaded when the fops are in use
	.open = RGBLed_open,
	.release = RGBLed_close,
	.read = RGBLed_read,
	.write = RGBLed_write,
	.unlocked_ioctl = RGBLed_ioctl
};



struct RGBLed_device
{
	struct cdev cdev;
	char name[20];
	int gpio_r, gpio_g, gpio_b;
	int color;
	int intensity;
} *rgbled;




// Timer callback function.
// This toggles the Led pin after on_time and off_time 
enum hrtimer_restart timer_callback( struct hrtimer *timer_for_restart )
{
  	ktime_t currtime , interval;
	long timer_interval_ns;
  	currtime  = ktime_get();
	
	if (on_time)
		timer_interval_ns = CYCLE_DURATION/100 * rgbled->intensity;	//on time
	else
		timer_interval_ns = CYCLE_DURATION - (CYCLE_DURATION/100 * rgbled->intensity);	//offtime

	enable_display(on_time);
	//printk("TIMER_INTERVAL_NS %ld\n", timer_interval_ns);		
	on_time = !on_time;
	interval = ktime_set(0, timer_interval_ns); 
	if(!exit){
  		hrtimer_forward(timer_for_restart, currtime , interval);
		return HRTIMER_RESTART;
	}
	else{	
		//if the file is closed, we don't restart the cycle. 
		return HRTIMER_NORESTART;
	}
}


// enable the leds according to the last three bits of color
void
enable_display(bool on){
	int r = 0, g = 0, b = 0;
	if(on){
		r =rgbled->color>>2 & 1;
		g =rgbled->color>>1 & 1;
		b =rgbled->color & 1;
	}
	gpio_set_value(pin_mux[rgbled->gpio_r][LINUX_PIN], r);
	gpio_set_value(pin_mux[rgbled->gpio_g][LINUX_PIN], g);
	gpio_set_value(pin_mux[rgbled->gpio_b][LINUX_PIN], b);
}


//configure the IO pins
void
configure_pin(int pin){
	gpio_request(pin_mux[pin][LINUX_PIN], "sysfs");  
	if(pin_mux[pin][DIRECTION_PIN]!=NaN){
		gpio_request(pin_mux[pin][DIRECTION_PIN], "sysfs");

	}
	if(pin_mux[pin][PIN_MUX]!=NaN){
		gpio_request(pin_mux[pin][PIN_MUX], "sysfs");
		gpio_export(pin_mux[pin][PIN_MUX], true); 
		//gpio_direction_output(pin_mux[pin][PIN_MUX], pin_mux[pin][VALUE]);	
	}
	
	gpio_direction_output(pin_mux[pin][LINUX_PIN], 0);
	gpio_export(pin_mux[pin][LINUX_PIN], true); 
	if(pin_mux[pin][DIRECTION_PIN]!=NaN){
		gpio_direction_output(pin_mux[pin][DIRECTION_PIN], 0);
		gpio_export(pin_mux[pin][DIRECTION_PIN], true); 
	}
}


//configure the three LED pins
void
IOSetup(int io_r, int io_g, int io_b){
	printk("Configuring pins %d %d %d\n", io_r, io_g, io_b);
	configure_pin(io_r);
	configure_pin(io_g);
	configure_pin(io_b);	
}


//Free GPIO pins
void
free_io(int io_r, int io_g, int io_b){
	//return gpio to system
	if(pin_mux[io_r][DIRECTION_PIN]!=NaN)
		gpio_free(pin_mux[io_r][DIRECTION_PIN]);
	if(pin_mux[io_g][DIRECTION_PIN]!=NaN)
		gpio_free(pin_mux[io_g][DIRECTION_PIN]);
	if(pin_mux[io_b][DIRECTION_PIN]!=NaN)
		gpio_free(pin_mux[io_b][DIRECTION_PIN]);
	if(pin_mux[io_r][PIN_MUX]!=NaN)
		gpio_free(pin_mux[io_r][PIN_MUX]);
	if(pin_mux[io_g][PIN_MUX]!=NaN)
		gpio_free(pin_mux[io_g][PIN_MUX]);
	if(pin_mux[io_b][PIN_MUX]!=NaN)
		gpio_free(pin_mux[io_b][PIN_MUX]);
	gpio_free(pin_mux[io_r][LINUX_PIN]);
	gpio_free(pin_mux[io_g][LINUX_PIN]);
	gpio_free(pin_mux[io_b][LINUX_PIN]);
}

//Called when a user opens the device file using sysfs. (Device file is under /dev/DEVICE_NAME)
int
RGBLed_open (struct inode *inod, struct file *filp)
{
	struct RGBLed_device *rgbled;
	long timer_interval_ns;
	ktime_t interval;
	exit = false;
	printk (KERN_INFO "RGBLed: Opening device RGBLed\n");
	rgbled = container_of (inod->i_cdev, struct RGBLed_device, cdev);
	filp->private_data = rgbled;
	
	//default values
	rgbled->intensity = 50;
	rgbled->gpio_r = 0;
	rgbled->gpio_g = 1;
 	rgbled->gpio_b = 2;
	IOSetup(rgbled->gpio_r, rgbled->gpio_g, rgbled->gpio_b);
	//Increase use count (no of processes that are using the module). 3rd field in /proc/modules
	try_module_get (THIS_MODULE);
	on_time = true;
	//initiate timer
	timer_interval_ns = CYCLE_DURATION/100 * rgbled->intensity;
	interval = ktime_set(0, timer_interval_ns);
	hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	timer.function = &timer_callback;
	hrtimer_start( &timer, interval, HRTIMER_MODE_REL );
	return 0;
}


//Close device and free all gpio pins
int
RGBLed_close (struct inode *inod, struct file *filp)
{
	struct RGBLed_device *devp = filp->private_data;
	printk (KERN_INFO "RGBLed: Closing device\n");
	//printk ("RGBLed: Color %d IO pins: %d %d %d Intensity: %d\n", devp->color, devp->gpio_r, devp->gpio_g, devp->gpio_b, devp->intensity);
	//Exit Hrtimer
	exit = true;
	hrtimer_cancel(&timer);
	free_io(devp->gpio_r, devp->gpio_g, devp->gpio_b);
	module_put (THIS_MODULE);	//decrement use count
	return 0;
}

//This operation is not supported by the device
ssize_t
RGBLed_read (struct file * file, char *buf, size_t count, loff_t * ppos)
{
	printk ("RGBLed: This operation is not supported!\n");
	return 0;
}

//Get an integer from the user and use the last three bits to turn on/off R, G, B
ssize_t
RGBLed_write (struct file * filp, const char *buf, size_t count,
loff_t * ppos)
{
	struct RGBLed_device *devp = filp->private_data;
	copy_from_user (&devp->color, buf, sizeof (int));
	on_time = true;
	return 0;
}


//Ioctl call
long
RGBLed_ioctl (struct file *filp, unsigned int command, unsigned long arg)
{
	int ret;
	int data[5];
	long timer_interval_ns;
	ktime_t interval;

	struct RGBLed_device *devp = filp->private_data;   
	switch(command){
		case CONFIG:    ret = copy_from_user(&data, (void*)arg, sizeof(data));
				free_io(devp->gpio_r, devp->gpio_g, devp->gpio_b);
				exit =true;
				hrtimer_cancel( &timer );
				devp->gpio_r = data[2];
				devp->gpio_g = data[3];
				devp->gpio_b = data[4];
				devp->intensity = data[1];
				if((ret < 0) | (data[1] > 100) | (data[1] < 0) ){
					printk("RGBLed: Invalid value for intensity\n");
					return -1;  
				}

				/*
				if((pin_mux[data[2]][LINUX_PIN]>15 )| (pin_mux[data[2]][LINUX_PIN] < 8)) {
					printk("RGBLed: Invalid value for pin. \n");
					return -1;  
				}
				if((pin_mux[data[3]][LINUX_PIN]>15 )| (pin_mux[data[3]][LINUX_PIN] < 8)){
					printk("RGBLed: Invalid value for pin. \n");
					return -1;  
				}
				if((pin_mux[data[4]][LINUX_PIN]>15) | (pin_mux[data[4]][LINUX_PIN] < 8)){
					printk("RGBLed: Invalid value for pin. \n");
					return -1;  
				}*/
				on_time = true;
				exit = false;
				timer_interval_ns = CYCLE_DURATION/100 * rgbled->intensity;
				interval = ktime_set(0, timer_interval_ns);
				hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
				timer.function = &timer_callback;
				hrtimer_start( &timer, interval, HRTIMER_MODE_REL );
				break;
		default: 	printk("Invalid Ioctl command");	
				return -1;

	}
	return 0;
}




int
RGBLed_init (void)
{
	//Major number: to identify the device driver used to handle the device
	//Minor number: to identify the individual devices handled by the driver(used by driver)

	// Request dynamic allocation of a device major number, first minor number, number of devices
	// device name is the name that appears under /proc/devices
	int ret;
	exit = false;
	if (alloc_chrdev_region (&major, 0, DEVICE_COUNT, DEVICE_NAME) < 0)
	{
		printk (KERN_DEBUG "Can't register device\n");
		return -1;
	}

	// * CHARACTER DEVICE REGISTRATION */
	rgbled = kmalloc (sizeof (struct RGBLed_device), GFP_KERNEL);
	if (!rgbled)
	{
		printk ("Could not allocate space in kernel for the device\n");
		return -ENOMEM;
	}
	sprintf (rgbled->name, DEVICE_NAME);
	//initialize cdev structure. pass the file operators struct associated with the device
	cdev_init (&rgbled->cdev, &RGBLed_fops);
	rgbled->cdev.owner = THIS_MODULE;
	//let kernel know about the initialized cdev struct 
	ret = cdev_add (&rgbled->cdev, major, DEVICE_COUNT);

	if (ret)
	{
		printk ("Could not add cdev structure to kernel\n");
		return ret;
	}

	/* CREATE DEVICE NODE UNDER /dev/ 
	Note: Must add MODULE_LICENSE("GPL") because class_create is exported only for 
	GPL license modules. Exported using EXPORT_SYMBOL_GPL instead of EXPORT_SYMBOL */
	RGBLed_class = class_create (THIS_MODULE, DEVICE_NAME);
	RGBLed_char_device =
	device_create (RGBLed_class, NULL, MKDEV (MAJOR (major), 0), NULL, DEVICE_NAME);
	return 0;
}

void
RGBLed_exit (void)
{
	//sys_delete_module would check the counter(no. of processes that are using the module)
	//remove module only if it's zero. 
	int ret = 0;

	//remove the char device
	cdev_del (&rgbled->cdev);

	device_destroy (RGBLed_class, major);
	class_destroy (RGBLed_class);

	//device numbers are freed
	unregister_chrdev_region (major, DEVICE_COUNT);
	

	kfree(rgbled);

	printk (KERN_INFO "Unregistered device!\n");

	if (ret < 0)
		printk (KERN_ALERT "Error in unregister_chrdev: %d\n", ret);
	exit = true;
	hrtimer_cancel(&timer);
	return;
}

module_init (RGBLed_init);
module_exit (RGBLed_exit);
MODULE_LICENSE ("GPL v2");

