
CC=i586-poky-linux-gcc
SROOT=/opt/iot-devkit/1.7.2/sysroots/i586-poky-linux

PATH := $(PATH):/opt/iot-devkit/1.7.2/sysroots/x86_64-pokysdk-linux/usr/bin/i586-poky-linux

all: assignment_1.c
	$(CC) -o assignment_1.o assignment_1.c -pthread

clean:
	rm -f *.o
