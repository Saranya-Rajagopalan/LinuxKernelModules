#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<stdint.h>
#include <linux/types.h>
#include<linux/input.h>
#include <linux/spi/spidev.h>
#include <poll.h>
#include<fcntl.h>
#include <signal.h>		// For event handling
#include "Gpio_func.h"
#include <stdlib.h>		// exit() function and exit codes like EXIT_FAILURE defined in this


#include <linux/ioctl.h>
#define MY_MAGIC 'G'
#define DISPLAY_PATTERN _IO(MY_MAGIC,1)
#define PASS_STRUCT_ARRAY_SIZE _IOW(MY_MAGIC,2,int)

#define	LINUX_PIN 0
#define DIRECTION_PIN 1
#define	PULL_UP 2
#define	PIN_MUX 3
#define	VALUE 4
#define	INTERRUPT_MODE 5
#define NaN 10000

#define DIRECTION_IN 0
#define DIRECTION_OUT 1
#define SAMPLING_TIME 500000

long delay=1000000;

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


typedef unsigned long long ticks;
int trigger_pin, echo_pin;
double current_distance =0, prev_distance =0;
int fd_spi, fd_ultrasonic;
int exit_condition = 0;
int seq_pointer =0;
pthread_t measure_distance, update_display;

//pin mux table for configuring gpio (not for any other functions)
static int pin_mux[20][6] ={{11, 32, 33, NaN, NaN, 0},			//IO0
			 {12, 28, 29, 45, 0, 0},			//IO1
			 {13, 34, 35, 77, 0, 0},			//IO2
			 {14, 16, 17, 76, 0, 1},			//IO3
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




void
IOSetup(){
	printf("Setting up IO... This will take a while. Please wait!\n");
	//setting up trigger and echo pins using pin mux table

	if(pin_mux[trigger_pin][DIRECTION_PIN]!=NaN){
		gpio_export(pin_mux[trigger_pin][DIRECTION_PIN]);
		gpio_set_value(pin_mux[trigger_pin][DIRECTION_PIN], DIRECTION_OUT);
	}	
	if(pin_mux[trigger_pin][PULL_UP]!=NaN)
		gpio_export(pin_mux[trigger_pin][PULL_UP]);
	if(pin_mux[trigger_pin][PIN_MUX]!=NaN){
		gpio_export(pin_mux[trigger_pin][PIN_MUX]);
		gpio_set_value(pin_mux[trigger_pin][PIN_MUX], pin_mux[trigger_pin][VALUE]);
	}

	gpio_export(pin_mux[trigger_pin][LINUX_PIN]);
	//echo pin
	//check if the pin actually supports interrupts
	if(pin_mux[echo_pin][INTERRUPT_MODE]==0){
		printf("Invalid Echo pin! Does not support both interrupts. Try again!\n");
		exit(EXIT_FAILURE);
	}
	if(pin_mux[echo_pin][DIRECTION_PIN]!=NaN){
		gpio_export(pin_mux[echo_pin][DIRECTION_PIN]);	
		gpio_set_value(pin_mux[echo_pin][DIRECTION_PIN], DIRECTION_IN);
	}
	if(pin_mux[echo_pin][PULL_UP]!=NaN)
		gpio_export(pin_mux[echo_pin][PULL_UP]);
	if(pin_mux[echo_pin][PIN_MUX]!=NaN){
		gpio_export(pin_mux[echo_pin][PIN_MUX]);
		gpio_set_value(pin_mux[echo_pin][PIN_MUX], pin_mux[echo_pin][VALUE]);
	}

	gpio_export(pin_mux[echo_pin][LINUX_PIN]);


}



void clear_display(){
	sequence[0].pattern =0;
	sequence[0].duration=0;
	write(fd_spi, &sequence, sizeof(sequence));
}

void cleanup() {
	exit_condition = 1;
	printf("Signal detected. Waiting for threads to exit\n");
	clear_display();
	pthread_kill(measure_distance, SIGINT);
	pthread_kill(update_display, SIGINT);
	printf("Unexporting gpio\n");
	close(fd_spi);
	if(pin_mux[trigger_pin][DIRECTION_PIN]!=NaN){
		gpio_unexport(pin_mux[trigger_pin][DIRECTION_PIN]);
	}	
	if(pin_mux[trigger_pin][PULL_UP]!=NaN)
		gpio_unexport(pin_mux[trigger_pin][PULL_UP]);
	if(pin_mux[trigger_pin][PIN_MUX]!=NaN){
		gpio_unexport(pin_mux[trigger_pin][PIN_MUX]);
	}

	gpio_unexport(pin_mux[trigger_pin][LINUX_PIN]);
	//echo
	if(pin_mux[echo_pin][DIRECTION_PIN]!=NaN){
		gpio_unexport(pin_mux[echo_pin][DIRECTION_PIN]);	
	}
	if(pin_mux[echo_pin][PULL_UP]!=NaN)
		gpio_unexport(pin_mux[echo_pin][PULL_UP]);
	if(pin_mux[echo_pin][PIN_MUX]!=NaN){
		gpio_unexport(pin_mux[echo_pin][PIN_MUX]);
	}

	gpio_unexport(pin_mux[echo_pin][LINUX_PIN]);
	close(fd_spi);
	close(fd_ultrasonic);
	exit(EXIT_SUCCESS);
}


//This function is called after every distance measurement in order to change the display pattern 
// in accordance with the distance of the obstacle from the device
void change_display_pattern(double dist ){
	current_distance =dist;
	double distance_interval = current_distance - prev_distance;

	if( (current_distance - prev_distance) > current_distance/10.0);

	if( (current_distance - prev_distance) < current_distance/10.0);
	if(current_distance <=50.0)
		delay = 30000;
	else
		delay = 50000;

	prev_distance = current_distance;
}

//code to convert timespec to nanoseconds and vice versa
//code reference from https://stackoverflow.com/questions/24389546/how-to-implement-timespec-accumulators
uint64_t
get_nanoseconds ()
{
	struct timespec current_time;
	clock_gettime (CLOCK_MONOTONIC, &current_time);
	return current_time.tv_sec * (uint64_t) 1000000000L + current_time.tv_nsec;
}



void* get_distance(void *arg) {
	int ret;
	uint64_t rising_edge_time, falling_edge_time; 
	struct pollfd edge_poll;
	char* buff[64];
	double distance;
	
	int fd_echo_value, fd_trigger_value, fd_echo_edge;

	fd_echo_value = gpio_fd_open_read(pin_mux[echo_pin][LINUX_PIN]);
	fd_trigger_value = gpio_fd_open_read(pin_mux[trigger_pin][LINUX_PIN]);


	edge_poll.fd = fd_echo_value;
	edge_poll.events = POLLPRI | POLLERR | POLLIN;
	edge_poll.revents=0; 

	while(!exit_condition){
		//FIRST POLL FOR RISING EDGE
		fd_echo_edge  = gpio_fd_open_edge(pin_mux[echo_pin][LINUX_PIN]);
		write(fd_echo_edge, "rising", sizeof("rising"));	
		falling_edge_time=0;
		rising_edge_time=0;
		lseek(fd_echo_value, 0, SEEK_SET);

		//triggering pulse
		write(fd_trigger_value,"0",1);
		usleep(14);
		write(fd_trigger_value,"1",1);
		usleep(14);
		write(fd_trigger_value,"0",1);
		//2000us is the timeout value
		ret = poll(&edge_poll, 1, 2000);
		if(ret>0){
		     rising_edge_time = get_nanoseconds();
		     read(fd_echo_value, buff, 1);
	      	}

		lseek(fd_echo_value, 0, SEEK_SET);

		//POLL FOR FALLING EDGE
		write(fd_echo_edge,"falling", sizeof("falling"));
		ret = poll(&edge_poll, 1, 2000);
		if(ret>0){
			/* record the time when falling edge is detected*/
			falling_edge_time = get_nanoseconds();
			read(fd_echo_value, buff, 1);
	}	
	distance =((falling_edge_time-rising_edge_time)*0.0000001*340.0)/2.00;
	printf("Distance measured in cm is %lf\n", distance);
	change_display_pattern(distance);
	usleep(SAMPLING_TIME);
	}
	return;
}


//This function is used to display the animation
void* display_animation(void *arg){
	printf("Displaying animation!\n");
	int i=0, j =0;

	while(!exit_condition){
		//Displaying patterns 0-9 for 1000ms if it's even pattern and for 2000ms if it's odd pattern
		for(i=0;i<10;i++){
			sequence[i].pattern = i;
			if(i%2==0)
				sequence[i].duration = 1000;
			else
				sequence[i].duration = 2000;
		}
		sequence[9].pattern =0; //turn off display matrix
		sequence[9].duration=0;
		write(fd_spi, &sequence, sizeof(sequence));
		sleep(1);
	}
}

int main(){
	
	int i=0, j=0;
	signal( SIGINT, cleanup);
	trigger_pin = 5; echo_pin = 6;
	printf("Trigger IO pin 5 Echo IO pin 6 by default!\n");
	
	IOSetup();
	fd_spi = open("/dev/MAX7219", O_RDWR);
	if (fd_spi < 0){
		perror("Can't open the device!");
		abort();
	}

	for(i=0;i<10;i++)
		for(j=0;j<8;j++)
			pattern_array[i].led[j] = display_pattern[i][j];
	
	//send the 10 patterns to spi device
	ioctl(fd_spi, DISPLAY_PATTERN, &pattern_array);
	
	printf("Starting display and distance measurement thread\n");
	
	pthread_create(&update_display, NULL, &display_animation, NULL);
	pthread_create(&measure_distance, NULL, &get_distance, NULL);
	pthread_join(update_display, NULL);
	pthread_join (measure_distance, NULL);
	cleanup();
	return 0;
}
