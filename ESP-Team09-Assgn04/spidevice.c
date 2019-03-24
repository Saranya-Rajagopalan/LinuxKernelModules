#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spi/pxa2xx_spi.h>
#include <linux/spi/spi.h>

struct spi_device *MAX7219_device;
struct spi_master *master;


static struct spi_board_info MAX7219_dev_info = {

	        .modalias = "MAX7219_driver",
		.max_speed_hz = 10*1000*1000,
		.mode = SPI_MODE_3,
		.bus_num = 1,
		.chip_select = 1,
};



static int MAX7219_device_init(void)
{
        master = spi_busnum_to_master( MAX7219_dev_info.bus_num );
	if( !master )
		return -ENODEV;

	MAX7219_device = spi_new_device( master, &MAX7219_dev_info );
	if( !MAX7219_device )
		return -ENODEV;
	printk("SPI_device init\n");

        return 0;

}

static void MAX7219_device_exit(void)
{
    	spi_unregister_device(MAX7219_device);

	printk(KERN_ALERT "Goodbye, unregister the device\n");
}

module_init(MAX7219_device_init);
module_exit(MAX7219_device_exit);
MODULE_LICENSE("GPL");
