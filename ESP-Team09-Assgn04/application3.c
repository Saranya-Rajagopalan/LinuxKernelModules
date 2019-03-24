#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<stdint.h>
#include <linux/types.h>
#include<linux/input.h>
#include <linux/spi/spidev.h>
#include <poll.h>
#include<fcntl.h>
#include <stdlib.h>		// exit() function and exit codes like EXIT_FAILURE defined in this
#include <linux/ioctl.h>
#define MY_MAGIC 'G'
#define DISPLAY_PATTERN _IO(MY_MAGIC,1)
#define PASS_STRUCT_ARRAY_SIZE _IOW(MY_MAGIC,2,int)



//name of the custom device device created 
static const char *device = "/dev/MAX7219";

//Display pattern
typedef struct{
	char led[8];
}PATTERN;

struct data{
	int pattern;
	int duration;
};

struct data sequence[10];

PATTERN pattern_array[10];

char display_pattern[10][8] = 
{{0b00000000, 0b00000000, 0b00111110, 0b01000001, 0b01000001, 0b00111110, 0b00000000, 0b00000000},//0
{ 0b00000000, 0b00000000,0b01000010, 0b01111111, 0b01000000, 0b00000000, 0b00000000, 0b00000000}, //1
{ 0b00000000, 0b00000000,0b01100010, 0b01010001, 0b01001001, 0b01000110, 0b00000000, 0b00000000}, //2
{ 0b00000000, 0b00000000,0b00100010, 0b01000001, 0b01001001, 0b00110110, 0b00000000, 0b00000000}, //3
{ 0b00000000, 0b00000000,0b00011000, 0b00010100, 0b00010010, 0b01111111, 0b00000000, 0b00000000}, //4
{ 0b00000000, 0b00000000,0b00100111, 0b01000101, 0b01000101, 0b00111001, 0b00000000, 0b00000000}, //5
{ 0b00000000, 0b00000000,0b00111110, 0b01001001, 0b01001001, 0b00110000, 0b00000000, 0b00000000}, //6
{ 0b00000000, 0b00000000,0b01100001, 0b00010001, 0b00001001, 0b00000111, 0b00000000, 0b00000000}, //7
{ 0b00000000, 0b00000000,0b0110110, 0b01001001, 0b01001001, 0b00110110, 0b00000000, 0b00000000}, //8
{ 0b00000000, 0b00000000,0b0110110, 0b01001001, 0b01001001, 0b00110110, 0b00000000, 0b00000000}}; //9


int main(int argc, char *argv[])
{
	int fd;
	int i=0, j =0;

	//Displaying patterns 0-9 for 1000ms if it's even pattern and for 2000ms if it's odd pattern
	for(i=0;i<10;i++){
		for(j=0;j<8;j++){
				pattern_array[i].led[j] = display_pattern[i][j];
				sequence[i].pattern = i;
				if(i%2==0)
					sequence[i].duration = 1000;
				else
					sequence[i].duration = 2000;
		}
	}
	sequence[9].pattern =0; //turn off display matrix
	sequence[9].duration=0;

	fd = open(device, O_RDWR);
	if (fd < 0){
		perror("Can't open the device!");
		abort();
	}
	
	if(ioctl(fd, PASS_STRUCT_ARRAY_SIZE, 10) < 0){
		perror("PASS_STRUCT_ARRAY_SIZE : ");
		return -1;
	}

	ioctl(fd, DISPLAY_PATTERN, &pattern_array);
	write(fd, &sequence, sizeof(sequence));
	close(fd);
	return 0;
}
