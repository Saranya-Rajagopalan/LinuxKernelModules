/*
 * SPI testing utility (using spidev driver)
 *
 * Copyright (c) 2007  MontaVista Software, Inc.
 * Copyright (c) 2007  Anton Vorontsov <avorontsov@ru.mvista.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 * Cross-compile with cross-gcc -I/path/to/cross-kernel/include
 */

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>


#define RESET 459 //IOCTL

static const char *device = "/dev/WS2812";
static uint32_t mode;


int main(int argc, char *argv[])
{
	int fd;
	int* no;

	fd = open(device, O_RDWR);
	if (fd < 0){
		perror("Can't open the device!");
		abort();
	}
	
	printf("Resetting the pin configurations to set IO11 to SPI MOSI\n");
	
	ioctl(fd, RESET);

	printf("Enter the number of LEDs to light\n");

	scanf("%d",&no);

	write(fd, &no, sizeof(no));

	printf("Resetting the pin configurations to set IO11 to SPI MOSI and clearing the LEDs\n");

	ioctl(fd, RESET);

	close(fd);

	return 0;
}
