/**
 * @file   testebbchar.c
 * @author Derek Molloy
 * @date   7 April 2015
 * @version 0.1
 * @brief  A Linux user space program that communicates with the ebbchar.c LKM. It passes a
 * string to the LKM and reads the response from the LKM. For this example to work the device
 * must be called /dev/ebbchar.
 * @see http://www.derekmolloy.ie/ for a full description and follow-up descriptions.
*/
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#pragma pack(1)
#define IOCTL_READ_FROM_MAXIMUM 899

struct rbtdevice_user
   {
    int key;
    int data;
}rb_object_t;

static char receive[sizeof(struct rbtdevice_user)];    ///< The receive buffer from the LKM

int main(){
   int ret, fd, i;

   printf("Starting device test code example...\n");
   fd = open("/dev/rbtdevice", O_RDWR);             // Open the device with read/write access
   if (fd < 0){
      perror("Failed to open the device...");
      return errno;
   }
   for(i = 0; i< 5; i++){
	   printf("Type key and data to send to the kernel module:\n");
	   scanf("%d %d",&rb_object_t.key, &rb_object_t.data);

	   char* pchar = malloc(sizeof(rb_object_t));
	   memcpy ( pchar, &rb_object_t, sizeof(rb_object_t) );

	   printf("Writing message to the device [%d], %d.\n",rb_object_t.key, rb_object_t.data);   

	   printf("Writing message to the device \n");
	   ret = write(fd,(char *) pchar, sizeof(rb_object_t)); // Send the string to the LKM
	   if (ret < 0){
	      perror("Failed to write the message to the device.");
	      return errno;
	   }
   }
 

   printf("Setting IOCTL_READ_FROM_MAXIMUM to 0\n. So reading from Min.\n");   
   ioctl(fd, IOCTL_READ_FROM_MAXIMUM, 0);
   
   printf("Reading from the device...\n");
   ret = read(fd, receive, sizeof(rb_object_t));        // Read the response from the LKM
   struct rbtdevice_user *rbt_r = malloc(sizeof(struct rbtdevice_user));
   memcpy (rbt_r, receive, sizeof(receive) );
   if (ret < 0){
      perror("Failed to read the message from the device.");
      return errno;
   }
   printf("The received message is: %d, %d\n", rbt_r->key, rbt_r->data);

   printf("Setting IOCTL_READ_FROM_MAXIMUM to 1\n. So reading from Max.\n");
   ioctl(fd, IOCTL_READ_FROM_MAXIMUM, 1);
   
   printf("Press enter to read\n");
   //getchar();
   printf("Reading from the device...\n");
   ret = read(fd, receive, sizeof(rb_object_t));        // Read the response from the LKM
   memcpy (rbt_r, receive, sizeof(receive) );
   if (ret < 0){
      perror("Failed to read the message from the device.");
      return errno;
   }
   printf("The received message is: %d, %d\n", rbt_r->key, rbt_r->data);
	
   printf("End of the program\n");
   return 0;
}
