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

	ssize_t read_bit, write_bit;
	uint32_t value;

	// open file
	int fd = open("/dev/hw3_network_driver", O_RDWR);
	if(fd < 0)
   		printf("ERROR! couldnt open file...\n");


	// read file
	printf("%08x\n",value);
   	read_bit = read(fd, &value, sizeof(uint32_t));
	if(read_bit < 0)
    printf("ISSUE WITH INITIALREAD ");
	printf("Value read from device: %08x\n", value);
	
	//LED ON
   	value = 0xE;

	//write file 
	write_bit = write(fd, &value, sizeof(uint32_t));
	if(write_bit < 0)
    printf("ISSUE WITH WRITE 1");
	printf("Value set to device: %08x\n", value);
	//read file 
	read_bit = read(fd, &value, sizeof(uint32_t));
	if(read_bit < 0)
    printf("ISSUE WITH READING FROM RECENTLY WRITTEN TO FILE ");
	printf("Value read from device: %08x\n", value);
	
	//wait 
	printf("waiting...\n");
	sleep(2);
	
	//LED OFF
	value = 0xF;

	//write file 
	write_bit = write(fd, &value, sizeof(uint32_t));
	if(write_bit < 0)
    printf("ISSUE WITH WRITE 2");
	printf("Value set to device: %08x\n", value);

   	read_bit = read(fd, &value, sizeof(uint32_t));
	if(read_bit < 0)
    printf("ISSUE WITH READ 2");
	printf("Value read from device: %08x\n", value);

	
	//close file
	close(fd);
	return 0;
}

