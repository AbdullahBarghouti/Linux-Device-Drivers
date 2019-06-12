/*****************************
Abdullah Barghouti 
ECE 373
HW 8 
Portland State University 2019
******************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdint.h>

int main(){

	int fd_open,fd_read,fd_write;
	uint32_t value;

 	fd_open = open("/dev/ece_led",O_RDWR);
	if(fd_open  < 0)
	{
		printf("ERROR! FILE OPEN FAILED\n");
	}

	//read from file 
	fd_read = read(fd_open, &value, sizeof(uint32_t));
	if(fd_read < 0)
	{
		printf("ERROR! FILE READ FAILD\n");
	}	

	printf("receieved tail value:  %d\n", (value & 0x00FF));
	value = value >> 8;
	printf("receieved head value: %d\n", value);

	
	

	close(fd_open);	
	return 0;

}

