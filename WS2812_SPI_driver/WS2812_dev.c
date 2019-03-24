

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spi/pxa2xx_spi.h>
#include <linux/spi/spi.h>

struct spi_device *WS2812_dev;
struct spi_master *master;


static struct spi_board_info WS2812_dev_info = {

	        .modalias = "WS2812_driver",
		.max_speed_hz = 6.4*1000*1000,
		.mode = SPI_MODE_3,
		.bus_num = 1,
		.chip_select = 1,
};



static int WS2812_device_init(void)
{
	int ret = 0; 
        master = spi_busnum_to_master( WS2812_dev_info.bus_num );
	if( !master )
		return -ENODEV;

	WS2812_dev = spi_new_device( master, &WS2812_dev_info );
	if( !WS2812_dev )
		return -ENODEV;

        return 0;

}

static void WS2812_device_exit(void)
{
    	spi_unregister_device(WS2812_dev);

	printk(KERN_ALERT "Goodbye, unregister the device\n");
}

module_init(WS2812_device_init);
module_exit(WS2812_device_exit);
MODULE_LICENSE("GPL");
