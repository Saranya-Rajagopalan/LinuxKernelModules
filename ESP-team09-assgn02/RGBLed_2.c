#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<linux/input.h>
#include<fcntl.h>
#include <signal.h>		// For event handling

#include "Gpio_func.h"

#define NaN 100000
#define MOUSEFILE "/dev/input/event2"
#define CYCLE_DURATION 20000 //in microseconds
#define STEP_DURATION 500000 //in microseconds
#define NO_OF_CYCLES STEP_DURATION/CYCLE_DURATION

//Index for pin multiplexing
#define	LINUX_PIN 0
#define DIRECTION_PIN 1
#define	PULL_UP 2
#define	PIN_MUX 3
#define	VALUE 4
#define	BOTH_INTERRUPT_SUPPORT 5



int cycle_stage;			    //current stage of the cycle. Can take values from 0-7
int duty_cycle, on_time, off_time, r, g, b; //duty_Cycle, r, g, b are the io pin values received from 
					    //the user
int red_fd, green_fd, blue_fd;	      	    //File descriptors for the linuxgpio sysfs interface

//pin mux table for configuring gpio (not for any other functions)
static int pin_mux[20][6] ={{11, 32, 33, NaN, NaN,0},			//IO0
			 {12, 28, 29, 45, 0, 0},			//IO1
			 {13, 34, 35, 77, 0, 0},			//IO2
			 {62, 16, 17, 76, 0, 1},			//IO3
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


//Setting up the io pins for r, g and b
void
IOSetup(){

	if(pin_mux[r][DIRECTION_PIN]!=NaN)
		mux_gpio_set(pin_mux[r][DIRECTION_PIN], 0);

	gpio_export(pin_mux[r][LINUX_PIN]);
	gpio_set_dir(pin_mux[r][LINUX_PIN], 1);

	if(pin_mux[g][DIRECTION_PIN]!=NaN)
		mux_gpio_set(pin_mux[g][DIRECTION_PIN], 0);

	gpio_export(pin_mux[g][LINUX_PIN]);
	gpio_set_dir(pin_mux[g][LINUX_PIN], 1);
	

	if(pin_mux[b][DIRECTION_PIN]!=NaN)
		mux_gpio_set(pin_mux[b][DIRECTION_PIN], 0);

	gpio_export(pin_mux[b][LINUX_PIN]);
	gpio_set_dir(pin_mux[b][LINUX_PIN], 1);
	

	if(pin_mux[r][PIN_MUX]!=NaN)
		mux_gpio_set(pin_mux[r][PIN_MUX], pin_mux[r][VALUE]);
	
	if(pin_mux[g][PIN_MUX]!=NaN)
		mux_gpio_set(pin_mux[g][PIN_MUX], pin_mux[g][VALUE]);

	if(pin_mux[b][PIN_MUX]!=NaN)
		mux_gpio_set(pin_mux[b][PIN_MUX], pin_mux[b][VALUE]);


	red_fd = gpio_fd_open(pin_mux[r][LINUX_PIN]);

	green_fd = gpio_fd_open(pin_mux[g][LINUX_PIN]);

	blue_fd = gpio_fd_open(pin_mux[b][LINUX_PIN]);

}


void 
switch_off_all(){
//function to turn off all the leds at once
	write(red_fd, "0", 1);
	write(green_fd, "0", 1);
	write(blue_fd, "0", 1);
}

//mouse event handler
void
mouse_event_handler (int sig)
{	
	cycle_stage = 0; //reset the cycle
	switch_off_all();
}

//to set the led values. The file descriptors for r, g, b gpio pins are global
// and are set by IOSetup
void
led_states(int r_value, int g_value, int b_value){
	int i;
	char r_str[2];
	char g_str[2];
	char b_str[2];
	sprintf(r_str, "%d", r_value);
	sprintf(g_str, "%d", g_value);
	sprintf(b_str, "%d", b_value);
	if(duty_cycle ==0){
		//if the duty_cycle is 0 just switch off the leds the whole time
		switch_off_all();
		usleep(CYCLE_DURATION);
		return;
	}
	//NO_OF_CYCLES are defined by macro and is dependent upon the CYCLE_DURATION 
	//AND STEP_DURATION
	for(i = 0; i < NO_OF_CYCLES; i++){
		write(red_fd, r_str, 1);
		write(green_fd, g_str, 1);
		write(blue_fd, b_str, 1);
		usleep(on_time);
		switch_off_all();
		usleep(off_time);
	}
}


//Looping thread
void* 
cycle(){
	while(1){
		switch(cycle_stage){
			case 0: led_states(0, 0, 0);
				break;
			case 1:	led_states(1, 0, 0); 
				break;
			case 2: led_states(0, 1, 0);
				break;
			case 3: led_states(0, 0, 1);
				break;
			case 4: led_states(1, 1, 0);
				break;
			case 5: led_states(1, 0, 1);
				break;
			case 6: led_states(0, 1, 1);
				break;
			case 7: led_states(1, 1, 1);
				break;
		}	
		cycle_stage++;
		cycle_stage = cycle_stage % 8;
	}
}

//Thread to read mouse events and call mouse_event_handler which resets the cycle
void *
read_mouse_event (void *arg)
{
	int fd;
	printf ("Mouse handler thread started!\n");
	struct input_event ie;
	if ((fd = open (MOUSEFILE, O_RDONLY)) == -1)
	{
		perror ("opening device");
		exit (EXIT_FAILURE);
	}
	while (read (fd, &ie, sizeof (struct input_event)))
	{

		if (ie.type == EV_KEY && ie.code == BTN_LEFT && ie.value == 0)
		{	
			duty_cycle += 10;
			if(duty_cycle > 100)
				duty_cycle = 100;
		}
		if (ie.type == EV_KEY && ie.code == BTN_RIGHT && ie.value == 0)
		{
			duty_cycle -= 10;
			if(duty_cycle < 0)
				duty_cycle = 0;
			
		}
		on_time = duty_cycle * CYCLE_DURATION / 100;
		off_time = CYCLE_DURATION - on_time;
		printf("Duty_cycle %d\n", duty_cycle);

	}
}


//cleanup everything
void 
cleanup(){
	switch_off_all();
	exit(1);
}


int main(int argc, char *argv[]){
	pthread_t mouse_thread, cycle_led_thread;
	signal (SIGUSR1, mouse_event_handler);
	signal( SIGINT, cleanup);

	if(argc != 5){
		printf("Incorrect number of arguments! Exiting!\n");
		return 0;
	}
	
	duty_cycle = atoi(argv[1]);
	r = atoi(argv[2]);
	g = atoi(argv[3]);
	b = atoi(argv[4]);


	printf("%d %d %d %d\n", duty_cycle, r, g, b);

	//Check if the command line variables are valid

	if(r < 0 | r > 13 | g < 0 | g > 13 | b < 0 | b > 13 | duty_cycle < 0 | duty_cycle > 100){
		printf("Invalid arguments! Exiting!\n");
		return 0;
	}

	IOSetup();

	on_time = duty_cycle * CYCLE_DURATION / 100;
	off_time = CYCLE_DURATION - on_time;

	//create two threads one for reading mouse events and one to cycle the LEDS
	pthread_create (&mouse_thread, NULL, read_mouse_event, NULL);
	pthread_create (&cycle_led_thread, NULL, cycle, NULL);

	pthread_join (mouse_thread, NULL);
	pthread_join (cycle_led_thread, NULL);

	return 0;
}
