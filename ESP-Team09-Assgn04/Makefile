CC = i586-poky-linux-gcc
ARCH = x86
CROSS_COMPILE = i586-poky-linux-


IOT_HOME = /opt/iot-devkit/1.7.2/sysroots


PATH := $(PATH):$(IOT_HOME)/x86_64-pokysdk-linux/usr/bin/i586-poky-linux

SROOT=$(IOT_HOME)/i586-poky-linux
APP1 = application1
APP2 = application2
APP3 = application3

obj-m = spidriver.o spidevice.o 

all:	
	$(CC) -c -o Gpio_func.o --sysroot=$(SROOT) Gpio_func.c 
	$(CC) -c -o application1.o --sysroot=$(SROOT) application1.c 
	$(CC) -c -o application2.o --sysroot=$(SROOT) application2.c 
	$(CC) -c -o application3.o --sysroot=$(SROOT) application3.c 
	$(CC) application1.o Gpio_func.o -o $(APP1) --sysroot=$(SROOT) -pthread
	$(CC) application2.o Gpio_func.o -o $(APP2) --sysroot=$(SROOT) -pthread
	$(CC) application3.o -o $(APP3) --sysroot=$(SROOT) -pthread
	make ARCH=x86 CROSS_COMPILE=i586-poky-linux- -C $(SROOT)/usr/src/kernel M=$(PWD) modules
	

clean:
	make ARCH=x86 CROSS_COMPILE=i586-poky-linux- -C $(SROOT)/usr/src/kernel M=$(PWD) clean
	rm -f *.o
