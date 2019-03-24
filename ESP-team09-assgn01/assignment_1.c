#include <stdio.h>		// For printing logs to console
#include <stdlib.h>		// exit() function and exit codes like EXIT_FAILURE defined in this
#include <pthread.h>		// For threading
#include <fcntl.h>		// Access specifier constants like O_RDONLY defined in this header
#include <linux/input.h>	// For input_event struct
#include <signal.h>		// For event handling
#include <semaphore.h>		//for semaphores
#include <string.h>		//for memset
#include <stdint.h>		//for uint64_t
#include <unistd.h>		//for read


#define MOUSEFILE "/dev/input/event6"

//#define MOUSEFILE "/dev/input/event2"
#define MAX_NO_OF_TASKS 10
#define LEFT_CLICK_EVENT 0
#define RIGHT_CLICK_EVENT 1
#define NaN 1000
#define randnum(min, max) ((rand() % (int)(((max) + 1) - (min))) + (min))


int current_event = NaN;	//Some value other than 0 and 1

sem_t activate_tasks;
sem_t right_click, left_click;

int n_right_clicks = 0, n_left_clicks = 0;
int exit_condition = 0;

void cleanup(void);

typedef struct task_parameters
{
	int tid;
	char type;
	struct sched_param schedule_parameter;
	int interrupt;
	int x_min;
	int x_max;
} task_parameters;



//timer for the whole program
void
sighandler_sigalarm (int sig)
{
	printf ("TIME OUT!!!\n");
	cleanup ();
	exit (EXIT_SUCCESS);
}

//mouse event handler
void
mouse_event_handler (int sig)
{	
	int i;
	//sem_post to all waiting threads and then reset the current_event global variable
	switch (current_event)
	{
		case LEFT_CLICK_EVENT:
			for(i =0; i<n_left_clicks; i++)
				sem_post (&left_click);	
			current_event = NaN;
			break;
		case RIGHT_CLICK_EVENT:
			for(i =0; i<n_right_clicks; i++)
				sem_post (&right_click);
			current_event = NaN;
			break;
	}
}

//Code to add signal handler for alarm. This will time out after specific time and rise SIGALRM signal which will 
// call the corresponding signal handler. 
//Code reference from https://www.unix.com/programming/143541-signals-semaphores.html
void
set_timer (int time_msec)
{
	struct itimerval timeout_it;
	struct sigaction signal_action;

	memset (&signal_action, 0, sizeof (signal_action));

	signal_action.sa_handler = sighandler_sigalarm;

	sigaction (SIGALRM, &signal_action, NULL);

	timeout_it.it_value.tv_sec = time_msec / 1000;
	timeout_it.it_value.tv_usec = (time_msec * 1000) % 1000000;
	timeout_it.it_interval = timeout_it.it_value;
	setitimer (ITIMER_REAL, &timeout_it, NULL);
}

//code to convert timespec to nanoseconds and vice versa
//code reference from https://stackoverflow.com/questions/24389546/how-to-implement-timespec-accumulators
uint64_t
get_nanoseconds (struct timespec *ts)
{
	return ts->tv_sec * (uint64_t) 1000000000L + ts->tv_nsec;
}

void
get_timespec (uint64_t time_ns, struct timespec *ts)
{
	ts->tv_sec = time_ns / (uint64_t) 1000000000L;
	ts->tv_nsec = time_ns % 1000000000L;
}


//Printing time stamp. Enter the thread id and TAG as inputs
void
print_timestamp (int tid, char string[])
{
	struct timespec current_time;
	clock_gettime (CLOCK_MONOTONIC, &current_time);
	printf ("Thread id %d: %s timestamp %lu.%lu\n", tid, string, current_time.tv_sec, current_time.tv_nsec);

}


//Generic periodic_task
void *
periodic_task (void *arg)
{
	task_parameters *parameters = (task_parameters *) arg;
	struct timespec till_when;
	int x = randnum (parameters->x_min, parameters->x_max);
	int i, j = 0;
	uint64_t total_nsec = 0;

	printf ("Thread id %d: Periodic task waiting for activation!\n",
	parameters->tid);
	sem_wait (&activate_tasks);
	printf ("Thread id %d: Periodic task Activated!\n", parameters->tid);

	clock_gettime (CLOCK_MONOTONIC, &till_when);

	while (1)
	{	print_timestamp(parameters->tid, "Task body start");
		j = 0;
		for (i = 0; i < x; i++)
		{
			j = j + i;
		}
		
		total_nsec = get_nanoseconds (&till_when) + parameters->interrupt;

		get_timespec (total_nsec, &till_when);

		clock_nanosleep (CLOCK_MONOTONIC, TIMER_ABSTIME, &till_when, 0);
		print_timestamp(parameters->tid, "Task body end");

	}

	printf ("Thread id %d: Periodic task exiting!\n", parameters->tid);

	pthread_exit (NULL);
}


//Generic aperiodic_task
void *
aperiodic_task (void *arg)
{
	task_parameters *parameters = (task_parameters *) arg;

	int x = randnum (parameters->x_min, parameters->x_max);
	int i, j = 0;

	if(parameters->interrupt!=LEFT_CLICK_EVENT && parameters->interrupt!=RIGHT_CLICK_EVENT){
		printf("Thread id %d: Incorrect event type(%d) Enter 0 for right click and 1 for left click!\n Start over. Exiting thread!\n", parameters->tid, parameters->interrupt);
		pthread_exit(NULL);
	}


	printf("Thread id %d: Aperiodic task waiting for activation! \n",parameters->tid);

	sem_wait (&activate_tasks);
	current_event = NaN;		//reset current_event 

	printf ("Thread id %d: Priority %d Aperiodic task Activated! \n", parameters->tid, parameters->schedule_parameter.sched_priority);
	printf("Thread id %d: Aperiodic task waiting for event %d!\n", parameters->tid, parameters->interrupt);

	if(parameters->interrupt==1)
		n_left_clicks++;
	else
		n_right_clicks++;

	while (1)
	{ 
		if(parameters->interrupt==1){
			sem_wait(&left_click);
			printf("Thread id %d: Signal received, entering task body!\n", parameters->tid);
		}
		else{
			sem_wait(&right_click);
			printf("Thread id %d: Signal received, entering task body!\n", parameters->tid);
		}
		print_timestamp(parameters->tid, "Task body start");
		j = 0;
		for (i = 0; i < x; i++)
		{
			j = j + i;
		}
		print_timestamp(parameters->tid, "Task body end");
	}

	printf ("Thread id %d: Aperiodic task Exiting!\n", parameters->tid);
	sem_post(&activate_tasks);

	pthread_exit (NULL);
}


// Thread which reads the mouse events and signals the signal handler which sends sem_post to all 
//threads waiting for it
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
	while (read (fd, &ie, sizeof (struct input_event)) && !exit_condition)
	{
		// type for mouse click is a key even
		// then check if it's right click or left click
		// value =0 indicates release
		// signal user defined signal upon desired event
		if (ie.type == EV_KEY && ie.code == BTN_LEFT && ie.value == 0)
		{
			current_event = LEFT_CLICK_EVENT;
			pthread_kill (pthread_self (), SIGUSR1);
		}
		if (ie.type == EV_KEY && ie.code == BTN_RIGHT && ie.value == 0)
		{
			current_event = RIGHT_CLICK_EVENT;
			pthread_kill (pthread_self (), SIGUSR1);
		}
	}
}


void
cleanup (void)
{
	sem_destroy (&activate_tasks);
	sem_destroy (&right_click);
	sem_destroy (&left_click);
}

int
main ()
{
	int n, timeout;
	int rc, i;
	char random_variable;
	task_parameters parameters[MAX_NO_OF_TASKS];
	pthread_t thr[MAX_NO_OF_TASKS];
	pthread_t mouse_thread;
	struct sched_param param;

	signal (SIGUSR1, mouse_event_handler);

	//initialize all semaphores needed for threads synchronization
	sem_init (&activate_tasks, 0, 0);
	sem_init (&right_click, 0, 0);
	sem_init (&left_click, 0, 0);


	printf
	("Enter the number of tasks and the timeout in ms separated by space\n");
	scanf ("%d %d", &n, &timeout);
	scanf("%c", &random_variable);
	printf ("%d %d\n", n, timeout);

	for (i = 0; i < n; i++)
	{
		scanf ("%c %d %d %d %d\n", &parameters[i].type,
		&parameters[i].schedule_parameter.sched_priority,
		&parameters[i].interrupt, &parameters[i].x_min,
		&parameters[i].x_max);
	}

	pthread_create (&mouse_thread, NULL, read_mouse_event, NULL);

	for (i = 0; i < n; ++i)
	{	parameters[i].tid = i;
		/// Somehow the following works only if the number of threads is less than 2. Could not identify the reason
		/*if(n<=2){ 
			pthread_setschedprio ((long)&thr[i],
				parameters[i].schedule_parameter.sched_priority);
			pthread_setschedparam ((long)&thr[i], SCHED_FIFO,
				&parameters[i].schedule_parameter);
		}*/
		switch (parameters[i].type)
			{
			case 'P'://if the type is P call periodic task
				if ((rc =
				pthread_create (&thr[i], NULL, periodic_task, &parameters[i])))
				{
					fprintf (stderr, "error: pthread_create, rc: %d\n", rc);
					return EXIT_FAILURE;
				}
				break;

			case 'A':// if the type is A call aperiodic task
				if ((rc =
				pthread_create (&thr[i], NULL, aperiodic_task,
					       &parameters[i])))
				{
					fprintf (stderr, "error: pthread_create, rc: %d\n", rc);
					return EXIT_FAILURE;
				}
				break;
			default:
				printf
				("Incorrect first argument. You should have given P for periodic task and A for Aperiodic task. Restart the program!\n");

			}
	}
		//Activate all threads at a time
	for(i =0; i<n; i++)
		sem_post (&activate_tasks);

	
	set_timer (timeout);

	for (i = 0; i < n; ++i)
	{
		pthread_join (thr[i], NULL);
		printf ("Thread id %d: Joining!\n", parameters[i].tid);
	}
	exit_condition =1;		//exit_condition for mouse thread. No point executing that if all other threads have joined main
	pthread_join (mouse_thread, NULL);
	cleanup ();
	return (0);
}

