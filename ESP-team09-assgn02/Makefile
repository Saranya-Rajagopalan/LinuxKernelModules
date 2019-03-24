CC=i586-poky-linux-gcc
SROOT=/opt/iot-devkit/1.7.2/sysroots/i586-poky-linux

PATH := $(PATH):/opt/iot-devkit/1.7.2/sysroots/x86_64-pokysdk-linux/usr/bin/i586-poky-linux

ARCH = x86
CROSS_COMPILE = i586-poky-linux-

APP1 = RGBLed1
APP2 = RGBLed2

all: RGBLed_1.o RGBLed_2.o Gpio_func.o
	$(CC) RGBLed_1.o Gpio_func.o -o $(APP1) --sysroot=$(SROOT) -pthread
	$(CC) RGBLed_2.o Gpio_func.o -o $(APP2) --sysroot=$(SROOT) -pthread

Gpio_func.o: Gpio_func.c 
	$(CC) -c -o Gpio_func.o --sysroot=$(SROOT) Gpio_func.c 

RGBLed_1.o: RGBLed_1.c
	$(CC) -c -o RGBLed_1.o --sysroot=$(SROOT) RGBLed_1.c 

RGBLed_2.o: RGBLed_2.c
	$(CC) -c -o RGBLed_2.o --sysroot=$(SROOT) RGBLed_2.c   

clean:
	rm -f *.o
	rm -f $(APP1) 
	rm -f $(APP2) 
