#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include <signal.h>
#include<pthread.h>
#include<linux/input.h>
#include<string.h>
#include <unistd.h>
#pragma pack(1)
#define CONFIG 899
#define MOUSEFILE "/dev/input/event2"
#define LED_DRIVER "/dev/RGBLed"

int cycle_stage =0;
int fd;

//mouse event handler
void
mouse_event_handler (int sig)
{	
	cycle_stage = 0; //reset the cycle
}

void
led_states(int r, int g, int b){
	int ret;
	int color = (0x01 & r) | (0x01 & g)<<1 | (0x01 & b)<<2;
	char* pchar = (char *)malloc(sizeof(int));
	memcpy ( pchar, &color, sizeof(int));

	ret = write(fd,(char *) pchar, sizeof(int)); 
	if (ret < 0){
		perror("Failed to write the message to the device.");
   	}
}

//Looping thread
void *
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
		cycle_stage = (cycle_stage+1) % 8;
		usleep(500000);
	}
	return(NULL);
}

//Thread to read mouse events and call mouse_event_handler which resets the cycle
void *
read_mouse_event (void *arg)
{
	int mouse_fd;
	printf ("TESTPROGRAM: Mouse handler thread started!\n");
	struct input_event ie;
	if ((mouse_fd = open (MOUSEFILE, O_RDONLY)) == -1)
	{
		perror ("opening device");
		exit (EXIT_FAILURE);
	}
	while (read (mouse_fd, &ie, sizeof (struct input_event)))
	{

		if (ie.type == EV_KEY && (ie.code == BTN_LEFT || ie.code == BTN_RIGHT)
			 && ie.value == 0)
			pthread_kill (pthread_self (), SIGUSR1);
	}
	return(NULL);
}

void
cleanup(){
	cycle_stage = 0;
	char* pchar = (char *)malloc(sizeof(int));
	memcpy ( pchar, &cycle_stage, sizeof(int));
	write(fd,(char *) pchar, sizeof(int)); 
	close(fd);
	exit(1);
}

int main(){
	int ret;
	pthread_t mouse_thread, cycle_led_thread;
	signal (SIGUSR1, mouse_event_handler);
	signal( SIGINT, cleanup);

	printf("TESTPROGRAM: Starting device test code example...\n");
	fd = open(LED_DRIVER, O_RDWR);             // Open the device with read/write access
	if (fd < 0){
		perror("Failed to open the device...");
		return errno;
	}


	int data[4];
	printf("TESTPROGRAM: Enter intensity, IO pin for Red, IO pin for Green and IO pin for Blue in order\n");
	scanf("%d %d %d %d", &data[0], &data[1], &data[2], &data[3]);
	char* pchar_ioctl = (char*)malloc(sizeof(data));
	memcpy ( pchar_ioctl, &data, sizeof(data) );
	ret = ioctl(fd, CONFIG, &pchar_ioctl);
	if(ret<0){
		printf("TESTPROGRAM: IOCTL failed!\n Exiting!\n");
		cleanup();
	}
	pthread_create (&mouse_thread, NULL, read_mouse_event, NULL);
	pthread_create (&cycle_led_thread, NULL, cycle, NULL);

	pthread_join (mouse_thread, NULL);
	pthread_join (cycle_led_thread, NULL);

	return 0;
}
