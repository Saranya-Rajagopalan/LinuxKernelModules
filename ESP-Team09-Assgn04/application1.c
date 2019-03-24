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


#define NaN 100000
#define TRIGGER_PULSE_DURATION 14

//Index for pin multiplexing
#define	LINUX_PIN 0
#define DIRECTION_PIN 1
#define	PULL_UP 2
#define	PIN_MUX 3
#define	VALUE 4
#define	INTERRUPT_MODE 5


#define DIRECTION_IN 0
#define DIRECTION_OUT 1

#define SPI_DEVICE "/dev/spidev1.0"
#define FRAME_TIME 1000000
#define SAMPLING_TIME 500000

#define FORWARD_1 1
#define FORWARD_2 2
#define BACKWARD_1 3
#define BACKWARD_2 4

#define DECODE 0x09
#define INTENSITY 0x0A
#define SCAN_LIMIT 0x0B
#define SHUTDOWN 0x0C
#define DISPLAY_TEST 0x0F

typedef unsigned long long ticks;
int trigger_pin, echo_pin;
double current_distance =0, prev_distance =0;
int fd_spi, fd_ultrasonic;
long delay=1000000;
int pattern = FORWARD_1;
struct spi_ioc_transfer spi_message;
uint8_t data[2];
int exit_condition = 0;
int exitted1 =1; int exitted2 =1;


//four display patterns

uint8_t display_pattern1[] =  { DECODE, 0x00, 
				DISPLAY_TEST, 0x00, 
				SHUTDOWN, 0x01, 
				INTENSITY, 0x0F, 
				SCAN_LIMIT, 0x07, 
				0x01, 0x06,
		        	0x02, 0x06, 
				0x03, 0x06, 
				0x04, 0x83, 
				0x05, 0x7F, 
				0x06, 0x24, 
				0x07, 0x22,
			        0x08, 0x63};

uint8_t display_pattern3[] =  { DECODE, 0x00, 
				DISPLAY_TEST, 0x00, 
				SHUTDOWN, 0x01, 
				INTENSITY, 0x0F, 
				SCAN_LIMIT, 0x07, 
				0x01, 0x06,
  		                0x02, 0x06, 
				0x03, 0x06, 
				0x04, 0x03, 
				0x05, 0x7F, 
				0x06, 0xA2, 
				0x07, 0x32,
 		                0x08, 0x16};

uint8_t display_pattern2[] =  { DECODE, 0x00, 
				DISPLAY_TEST, 0x00, 
				SHUTDOWN, 0x01, 
				INTENSITY, 0x0F, 
				SCAN_LIMIT, 0x07, 
				0x01, 0x60,
		                0x02, 0x60, 
				0x03, 0x60, 
				0x04, 0xC1, 
				0x05, 0xFE, 
				0x06, 0x24, 
				0x07, 0x44,
		                0x08, 0xC6};

uint8_t display_pattern4[] = {  DECODE, 0x00, 
				DISPLAY_TEST, 0x00, 
				SHUTDOWN, 0x01, 
				INTENSITY, 0x0F, 
				SCAN_LIMIT, 0x07, 
				0x01, 0x60,
		                0x02, 0x60, 
				0x03, 0x60, 
				0x04, 0xC0, 
				0x05, 0xFE, 
				0x06, 0x45, 
				0x07, 0x4C,
		                0x08, 0x68};

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
	//spi device
	gpio_export(5);
	gpio_export(15);
	gpio_export(7);
	gpio_export(24);
	gpio_export(42);
	gpio_export(30);
	gpio_export(25);
	gpio_export(43);
	gpio_export(31);
	gpio_export(44);
	gpio_export(46);
	gpio_set_dir(5, DIRECTION_OUT);
	gpio_set_value(5, 1);
	gpio_set_dir(15, DIRECTION_OUT);
	gpio_set_value(15, 0);
	gpio_set_dir(7, DIRECTION_OUT);
	gpio_set_value(7, 0);
	gpio_set_dir(24, DIRECTION_OUT);
	gpio_set_value(24, 0);
	gpio_set_dir(42, DIRECTION_OUT);
	gpio_set_value(42, 0);
	gpio_set_dir(30, DIRECTION_OUT);
	gpio_set_value(30, 0);
	gpio_set_dir(25, DIRECTION_OUT);
	gpio_set_value(25, 0);
	gpio_set_dir(43, DIRECTION_OUT);
	gpio_set_value(43, 0);
	gpio_set_dir(31, DIRECTION_OUT);
	gpio_set_value(31, 1);
	gpio_set_dir(44, DIRECTION_OUT);
	gpio_set_value(44, 1);
	gpio_set_dir(46, DIRECTION_OUT);
	gpio_set_value(46, 1);
	


	//setting up trigger and echo pins using pin mux table
	//trigger
	/*if(pin_mux[trigger_pin][INTERRUPT_MODE]==0){
		printf("Invalid Trigger pin! Does not support both interrupts. Try again!\n");
		exit(EXIT_FAILURE);
	}*/
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


int spi_display_setup(){
	unsigned char mode = 0;
	fd_spi = open(SPI_DEVICE, O_WRONLY);
		if(fd_spi<0)
	{
		printf("Can't open file %s!", SPI_DEVICE);
		exit(-1);
	}

	mode = mode | SPI_MODE_0;

	if(ioctl(fd_spi, SPI_IOC_WR_MODE, &mode) < 0)
	{
		perror("Setting mode failed for SPI\n");
		close(fd_spi);
		return -1;
	}


	// Setup a Write Transfer
	data[0] = 0x09; // Decode Mode
	data[1] = 0x00; // NO decode
	
	// Setup Transaction 
	spi_message.tx_buf = (int)data;
	spi_message.rx_buf = (int)NULL;
	spi_message.len = 2; // Bytes to send
	spi_message.cs_change = 0;
	spi_message.delay_usecs = 0;
	spi_message.speed_hz = 100000000;
	spi_message.bits_per_word = 8;
	
	// Send Message
	if(ioctl(fd_spi, SPI_IOC_MESSAGE(1), &spi_message) < 0)
		perror("SPI Message Send Failed");
	
	
	data[0] = 0x0a; // Intensity
	data[1] = 0x0F; // INtensity value
	spi_message.tx_buf = (int)data;
	if(ioctl(fd_spi, SPI_IOC_MESSAGE(1), &spi_message) < 0)
		perror("SPI Message Send Failed");

	data[0] = 0x0b; // Scan limit
	data[1] = 0x07; // NO limit, all digits

	spi_message.tx_buf = (int)data;
	if(ioctl(fd_spi, SPI_IOC_MESSAGE(1), &spi_message) < 0)
		perror("SPI Message Send Failed");

	data[0] = 0x0c; // SHutdown
	data[1] = 0x01; // OFF
	spi_message.tx_buf = (int)data;
	if(ioctl(fd_spi, SPI_IOC_MESSAGE(1), &spi_message) < 0)
		perror("SPI Message Send Failed");

	data[0] = 0x0f; // Display Test
	data[1] = 0x00; // OFF
	spi_message.tx_buf = (int)data;
	if(ioctl(fd_spi, SPI_IOC_MESSAGE(1), &spi_message) < 0)
		perror("SPI Message Send Failed");

	return 0;

}

void clear_display(){
	int count = 0;
	printf("Clearing Display\n");
	uint8_t row[2];


	struct spi_ioc_transfer message = {
		.tx_buf = (unsigned long)row,
		.len = 2,
		.speed_hz = 100*1000*1000,
		.bits_per_word = 8,
		.cs_change = 0,
	};

	while(count<8){
		row[0] = count<<2;
		row[1] = 0b00000000;
	
		//CS of the led display is set to LOW to enable device
		gpio_set_value(15, 0);
		if(ioctl(fd_spi, SPI_IOC_MESSAGE (1), &message)<0)
			printf("Error sending message\n");
		usleep(1);
		//CS of the led display is set to HIGH to disable device
		gpio_set_value(15, 1);
		count++;
	}
	if(ioctl(fd_spi, SPI_IOC_MESSAGE (1), &message) <0)
		printf("ERROR IN IOCTL! \n");
	close (fd_spi);
	return;
}

void cleanup() {
	exit_condition = 1;
	printf("Signal detected. Waiting for threads to exit\n");
	while(!exitted1 || !exitted2);
	clear_display();
	printf("Unexporting gpio\n");
	gpio_unexport(5);
	gpio_unexport(15);
	gpio_unexport(7);
	gpio_unexport(24);
	gpio_unexport(42);
	gpio_unexport(30);
	gpio_unexport(25);
	gpio_unexport(43);
	gpio_unexport(31);
	gpio_unexport(44);
	gpio_unexport(46);
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
	exit(EXIT_SUCCESS);
}


//This function is called after every distance measurement in order to change the display pattern 
// in accordance with the distance of the obstacle from the device
void change_display_pattern(double dist ){
    current_distance =dist;
    double distance_interval = current_distance - prev_distance;
    
	// if the current distance is greater than previous distance, then move backward or move forward
   if( (current_distance - prev_distance) > current_distance/10.0)
	pattern = BACKWARD_1;

   if( (current_distance - prev_distance) < current_distance/10.0)
	pattern = FORWARD_1;
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
	exitted2 = 0;
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
	exitted2 = 1;
	return;
}


//This function is used to display the animation
void* display_animation(void *arg){
	int count = 0;
	uint8_t row[2];
	int exitted1 =0;

	struct spi_ioc_transfer message = {
		.tx_buf = (int)row,
		.len = 2,
		.speed_hz = 10*1000*1000,
		.bits_per_word = 8,
		.cs_change = 0,
	};
	gpio_set_value(15, 0);

	while(!exit_condition){
		count = count % 26 ;
		
		//printf("PATTERN %d COUNT %d\n",pattern, count);
		switch(pattern){
			case FORWARD_1:	row[0] = display_pattern1[count];
					row[1] = display_pattern1[count+1];
					if(count==24)
						pattern = FORWARD_2;
					break;
			case FORWARD_2: row[0] = display_pattern2[count];
					row[1] = display_pattern2[count+1];
					if(count==24)
						pattern = FORWARD_1;
					break;
			case BACKWARD_1:row[0] = display_pattern3[count];
					row[1] = display_pattern3[count+1];
					if(count==24)
						pattern = BACKWARD_1;
					break;
			case BACKWARD_2: row[0] = display_pattern4[count];
					row[1] = display_pattern4[count+1];
					if(count==24)
						pattern = BACKWARD_2;

		}
		//CS of the led display is set to LOW to enable device
		gpio_set_value(15, 0);
		if(exit_condition)
			goto exitted;
		if(ioctl(fd_spi, SPI_IOC_MESSAGE (1), &message)<0)
			printf("Error sending message\n");
		usleep(10000);
		//CS of the led display is set to HIGH to disable device
		gpio_set_value(15, 1);
		count = count + 1;
		if(count==26){	
			//printf("COUNT 26\n");
			usleep(FRAME_TIME);	
		}
		
	}
	//if(ioctl(fd_spi, SPI_IOC_MESSAGE (1), &message) <0)
	//	printf("ERROR IN IOCTL! \n");
	//close (fd_spi);
exitted: exitted1 = 1;
	return;
}

int main(){
	pthread_t measure_distance, update_display;
	

	signal( SIGINT, cleanup);


	trigger_pin = 5; echo_pin = 6;
	printf("Trigger IO pin 5 Echo IO pin 6 by default!\n");
	
	IOSetup();
	spi_display_setup();
	
	printf("Starting display and distance measurement thread\n");
	
	pthread_create(&update_display, NULL, &display_animation, NULL);
	pthread_create(&measure_distance, NULL, &get_distance, NULL);
	pthread_join(update_display, NULL);
	pthread_join (measure_distance, NULL);
	cleanup();
	return 0;
}
