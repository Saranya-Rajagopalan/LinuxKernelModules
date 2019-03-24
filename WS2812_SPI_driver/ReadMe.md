# WS2812_SPI_driver
Linux SPI device driver to interface Neopixel  LED ring WS2812 with Intel Galileo Gen 2 board
#---------------------------------------------------------------------------------------------------------------

WS2812_driver is the name of the SPI driver that is created and WS2812 is the character device that's created. 
The Character device has a write file operation which takes an integer value n from the user to light n leds
in a circular fashion for 5 times. Any value over the maximum number of pixels(16) will be taken as the 
maximum value(16). ioctl has one command RESET to reset the spi mode and the led pixels and also to set the
gpio pin configuration.
#-----------------------------------------------------------------------------------------------------------------

Instructions to run:
1. Download and put everything in the same directory.
2. Open terminal in the same directory and enter "make" in superuser mode.
3. You will see that the object files are created in the directory. 
4. Insert the driver module and the device module using "sudo insmod WS2812_drv.ko" and "sudo insmod WS2812_dev.ko". 
5. Look for WS2812 under /dev directory to verify if the device is created.
6. Connect the WS2812 LED ring's data pin to IO11. Also connect the Vcc and ground. This driver module does not 
use the MOSI pin of the device. 
7. Run the test program a.out

#------------------------------------------------------------------------------------------------------------------

The test program does not cover the unit test. It is only a simple user program to use the device driver to write 
to the LED device. 

#-----------------EOF----------------------------------------------------------------------------------------------

