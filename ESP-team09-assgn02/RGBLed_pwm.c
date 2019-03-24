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
#define	PIN_MUX_1 3
#define	VALUE_1 4
#define PIN_MUX_2 5
#define	VALUE_2 6


//pin mux table for configuring gpio (not for any other functions)
static int pin_mux[12][7] ={{NaN, NaN, NaN, NaN, NaN, NaN, NaN},	//IO0
			 {NaN, NaN, NaN, NaN, NaN, NaN, NaN},		//IO1
			 {NaN, NaN, NaN, NaN, NaN, NaN, NaN},		//IO2
			 {1, 16, 17, 76, 0, 64, 1 },			//IO3
			 {NaN, NaN, NaN, NaN, NaN, NaN, NaN},		//IO4
			 {3, 18, 19, 66, 1, NaN, NaN},			//IO5
			 {5, 20, 21, 68, 1,  NaN, NaN},			//IO6
			 {NaN, NaN, NaN, NaN, NaN, NaN, NaN},		//IO7
			 {NaN, NaN, NaN, NaN, NaN, NaN, NaN},		//IO8
			 {7, 22, 23, 70, 0, NaN, NaN},			//IO9
			 {11, 26, 27, 74, 1, NaN, NaN},			//IO10
			 {9, 24, 25, NaN, NaN, 72, 1},			//IO11
			};


int cycle_stage;			    //current stage of the cycle. Can take values from 0-7
unsigned int duty_cycle, on_time, off_time, r, g, b; //duty_Cycle, r, g, b are the io pin values received from 
					    //the user
int red_fd, green_fd, blue_fd;	      	    //File descriptors for the linuxgpio sysfs interface


void 
switch_off_all(){
	write(red_fd, "0", 1);
	write(green_fd, "0", 1);
	write(blue_fd, "0", 1);
}



void
led_states(int r_value, int g_value, int b_value){
	int i;
	char r_str[2];
	char g_str[2];
	char b_str[2];
	if(duty_cycle ==0){
		switch_off_all();
		usleep(CYCLE_DURATION);
		return;
	}
	write(red_fd, r_str, 1);
	write(green_fd, g_str, 1);
	write(blue_fd, b_str, 1);
}


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

int
set_duty_cycle(){
	int r_fd, g_fd, b_fd, len;
	char rbuf[MAX_BUF], gbuf[MAX_BUF], bbuf[MAX_BUF];
	char duty_cycle[MAX_BUF];
	len = snprintf(rbuf, sizeof(rbuf), "/sys/class/pwm/pwmchip0/pwm%d/duty_cycle", pin_mux[r][0]);
	len = snprintf(gbuf, sizeof(gbuf), "/sys/class/pwm/pwmchip0/pwm%d/duty_cycle", pin_mux[g][0]);
	len = snprintf(bbuf, sizeof(bbuf), "/sys/class/pwm/pwmchip0/pwm%d/duty_cycle", pin_mux[b][0]);
	len = snprintf(duty_cycle, sizeof(duty_cycle), "%d", duty_cycle);
	r_fd = open(rbuf, O_WRONLY);
	g_fd = open(gbuf, O_WRONLY);
	b_fd = open(bbuf, O_WRONLY);
	if (r_fd < 0 | g_fd < 0 | b_fd < 0) {
		perror("gpio/set-value");
		return -1;
	}
	write(r_fd, duty_cycle, len);
	write(g_fd, duty_cycle, len);
	write(b_fd, duty_cycle, len);
	close(r_fd);
	close(g_fd);
	close(b_fd);
	return 0;

}


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
		set_duty_cycle();

	}
}

int pwm_export(unsigned int pwm)
{
	int fd, len;
	char buf[MAX_BUF];
 
	fd = open("/sys/class/pwm/pwmchip0/export", O_WRONLY);
	if (fd < 0) {
		perror("pwm/export");
		return fd;
	}
 
	len = snprintf(buf, sizeof(buf), "%d", pwm);
	write(fd, buf, len);
	close(fd);
	return 0;
}

int pwm_set_period(unsigned long period){
	int fd, len;
	char buf[MAX_BUF];
 
	fd = open("/sys/class/pwm/pwmchip0/device/pwm_period", O_WRONLY);
	if (fd < 0) {
		perror("pwmchip0/device/pwm_period");
		return fd;
	}
 
	len = snprintf(buf, sizeof(buf), "%ld", period);
	write(fd, buf, len);
	close(fd);
	return 0;
}


int pwm_fd_open(unsigned int pwm)
{
	int fd, len;
	char buf[MAX_BUF];

	len = snprintf(buf, sizeof(buf), "/sys/class/pwm/pwmchip0/pwm%d/enable", pwm);
 
	fd = open(buf, O_RDONLY | O_WRONLY| O_NONBLOCK );
	if (fd < 0) {
		printf("%d", pwm);
		perror("gpio/fd_open");
	}
	return fd;
}

void 
IOSetup(){
	pwm_export(pin_mux[r][0]);
	pwm_export(pin_mux[g][0]);	
	pwm_export(pin_mux[b][0]);	
	pwm_set_period(20000000);
	red_fd = pwm_fd_open(pin_mux[r][0]);
	green_fd = pwm_fd_open(pin_mux[g][0]);
	blue_fd	= pwm_fd_open(pin_mux[b][0]);

	if(pin_mux[r][PIN_MUX_1]!=NaN)
		mux_gpio_set(pin_mux[r][PIN_MUX_1], pin_mux[r][VALUE_1]);
	
	if(pin_mux[g][PIN_MUX_1]!=NaN)
		mux_gpio_set(pin_mux[g][PIN_MUX_1], pin_mux[g][VALUE_1]);

	if(pin_mux[b][PIN_MUX_1]!=NaN)
		mux_gpio_set(pin_mux[b][PIN_MUX_1], pin_mux[b][VALUE_1]);

	if(pin_mux[r][PIN_MUX_2]!=NaN)
		mux_gpio_set(pin_mux[r][PIN_MUX_2], pin_mux[r][VALUE_2]);
	
	if(pin_mux[g][PIN_MUX_2]!=NaN)
		mux_gpio_set(pin_mux[g][PIN_MUX_2], pin_mux[g][VALUE_2]);

	if(pin_mux[b][PIN_MUX_2]!=NaN)
		mux_gpio_set(pin_mux[b][PIN_MUX_2], pin_mux[b][VALUE_2]);

	set_duty_cycle();

}

void 
cleanup(){
	switch_off_all();
	exit(1);
}


int main(int argc, char *argv[]){
	pthread_t mouse_thread, cycle_led_thread;
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

	if(r < 0 | r > 11 | g < 0 | g > 11 | b < 0 | b > 11 | duty_cycle < 0 | duty_cycle > 100){
		printf("Invalid arguments! Exiting!\n");
		return 0;
	}

	if(pin_mux[r][LINUX_PIN]==NaN | pin_mux[g][LINUX_PIN]==NaN | pin_mux[b][LINUX_PIN]==NaN ){
		printf("Invalid pin number! Exiting!\n");
		return 0;
	}
		

	IOSetup();

	on_time = duty_cycle * CYCLE_DURATION / 100;
	off_time = CYCLE_DURATION - on_time;


	pthread_create (&mouse_thread, NULL, read_mouse_event, NULL);
	pthread_create (&cycle_led_thread, NULL, cycle, NULL);

	pthread_join (mouse_thread, NULL);
	pthread_join (cycle_led_thread, NULL);

	return 0;
}
