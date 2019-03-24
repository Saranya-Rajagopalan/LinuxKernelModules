/*
 BASIC COMMAND LINE PROGRAM TO USE THE GMEM DRIVER.

 It has two main uses, read and write.
 Both these functions are used to read and write to the driver.

 */

#include<stdio.h>
#include<fcntl.h>
#include<string.h>

#define DRIVER_NAME "/dev/rbtdevice"
#define KPROBE "/dev/RBProbe"

struct rb_node_user
   {
    int key;
    int data;
}rb_object_t;

static char receive[sizeof(struct rb_node_user)];    ///< The receive buffer from the LKM

struct user_input{
   unsigned int offset;
   int flag;
   int function_type;
}user_value;

struct trace_data{
      long int pid;
      unsigned long int time_stamp;
      long int address;
}rd;


int main(int argc, char *argv[]) {
	int determine_command(char *);//determine which command was input...ie either help/read/write
	void format_output(int, char *);

	int choice = -1;
	char *argv2 = "";

	switch (argc) {
	case 1:
		printf(
				"Test function: you must specify either read or write or kprobe\nPlease type gmem_test -help for further details\n");
		return 0;

	case 2:
	case 3:
		choice = determine_command(argv[1]);

		if (argc > 2)//if more than 2 command line arguments then either a no or string was passed for read/write
			argv2 = argv[2];//since argv is local to main() only store it and pass it as parameters

		format_output(choice, argv2);
		return 0;

	default:
		printf(
				"Gmem_test: too many arguments\nPlease type gmem_test -help for usage information\n");
		return 0;

	}

	//	/if(argc>2)
//		argv2=argv[2];

//		format_output(choice,argv2);
	return 0;
}

int determine_command(char *string) {

	if (!strcmp(string, "-help") || (!strcmp(string, "help")))//many options to enable legacy input and avoid frustration
		return 1;

	if (!strcmp(string, "-read") || (!strcmp(string, "read"))
			|| (!strcmp(string, "-show")) || (!strcmp(string, "show")))
		return 2;

	if (!strcmp(string, "-write") || (!strcmp(string, "write")))
		return 3;
	if (!strcmp(string, "-kprobe") || (!strcmp(string, "kprobe")))
		return 4;

	return -1;
}

void format_output(int choice, char *argv2) {
	void print_help(void);
	void read_driver(char *);
	void write_driver(char *);
	void kprobe();

	switch (choice) {
	case -1:
		printf("Invalid command. Type gmem_test -help for details\n");
		return;

	case 1:
		print_help();
		return;

	case 2:
		read_driver(argv2);
		return;

	case 3:
		write_driver(argv2);
		return;
	case 4:
		kprobe();
		return;
	}
}

void print_help(void) {
	printf(
			"\Test: A helper program for the driver gmem\nPossible usage is test[option] <string>\n\n");
	printf("Option\t\t\tDescription\n-----\t\t\t-----------\n\n");
	printf(
			"-help\t\t\tDisplay the help\n-read/show x\t\tShow the contents of the driver specified by x bytes. If blank -> displays the entire content\n");
	printf(
			"-write <string>\t\twrite a string to the driver specified in <string>\n\n");
		printf(
			"-kprobe to register or unregister a kprobe to a function\n\n");
	return;
}

void read_driver(char *string) {
	int ret;
	int fp = open(DRIVER_NAME, O_RDWR);
	printf("Reading from the device...\n");
	struct rb_node_user *rbt_r = malloc(sizeof(struct rb_node_user));
	ret = read(fp, receive, sizeof(rb_object_t));        // Read the response from the LKM
	memcpy (rbt_r, receive, sizeof(receive) );
	if (ret < 0){
	perror("Failed to read the message from the device.");
	return ret;
	}
	printf("The received message is: %d, %d\n", rbt_r->key, rbt_r->data);
}

void write_driver(char *string) {
	int LENGTH = -1;
	int ret = 0;
	//printf("Write_driver has been called with string %s\n",string);	DEBUGGING

	   int fp = open(DRIVER_NAME, O_RDWR);

	   printf("Type key and data to send to the kernel module:\n");
	   scanf("%d %d",&rb_object_t.key, &rb_object_t.data);

	  // char* pchar;
	   //memcpy ( pchar, &rb_object_t, sizeof(rb_object_t) );

	   printf("Writing message to the device [%d], %d.\n",rb_object_t.key, rb_object_t.data);   

	   printf("Writing message to the device \n");
	   ret = write(fp, &rb_object_t, sizeof(rb_object_t)); // Send the string to the LKM
	   if (ret < 0){
	      perror("Failed to write the message to the device.");
	      return ret;
	   }
}


void kprobe(){

	int fd,ret;
	int choice;

	fd = open("/dev/RBProbe", O_RDWR);             // Open the device with read/write access
	   if (fd < 0){
	      perror("Failed to open the device...");
	      return ret;
	     }
	printf("Enter 0: Read function of KProbe 1: Write function of KProbe\n");
	scanf("%d", &choice);
	switch(choice){
	case 1: {
		printf("Enter 1: to register probe 0: unregister probe\n");
		scanf("%i",&user_value.flag);
		printf("Enter the function to probe \n1: rbtdevice_write_driver \n0: for rbtdevice_read_driver\n");
		scanf("%d",&user_value.function_type);
		printf("Enter the offset");
		scanf("%d",&user_value.offset);
		user_value.offset = 0;
		ret = write(fd, &user_value, sizeof(struct user_input));

		if (ret < 0){
			      perror("Failed to write the message to the device.");
			      return ret;
			   }
		break;
		}
	case 0:{

		ret = read(fd, &rd, sizeof(struct trace_data));

		printf("pid = %d timestamp = %lu, Address =0x%p \n", rd.pid , rd.time_stamp, rd.address);
		break;
		}
	
	default: printf("Invalid Input\n");
	}
	return 0;
}

