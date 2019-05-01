/*
Abdullah Barghouti 
ECE 373 HW 2
Portland State University 2019
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

int main()
{

	char newCharBuffer[40];
	char currentCharBuffer[40];
	int fd, value;


	// open file
	fd = open("/dev/char_driver", O_RDWR);
	if(fd < 0)
   		printf("ERROR! couldnt open file...\n");

	// read file
	int ret = read(fd, currentCharBuffer, sizeof(int));
	memcpy(&value, currentCharBuffer, sizeof(int) * sizeof(char));
	printf("\nRead from device: %d\n", value);

	//get user's value
	printf("Enter new value to send to the device ");
	fgets(newCharBuffer, 40, stdin);
	printf("the new value that was sent to the driver is: %s\n",newCharBuffer);

	//close file
	close(fd);
	return 0;
}

