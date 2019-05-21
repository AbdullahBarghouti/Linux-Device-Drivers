/*
Abdullah Barghouti 
ECE 373 HW 4
Portland State University 2019
*/

//libraries 
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <stdint.h>

//global variables 
static int dev_mem_fd;
#define LED_ZERO_OFF 	0xF
#define LED_ZERO_ON		0xE
#define LED_ONE_OFF		0xF00
#define LED_ONE_ON		0xE00
#define LED_TWO_OFF		0xF0000
#define LED_TWO_ON		0xE0000
#define LED_THREE_OFF	0xF000000
#define LED_THREE_ON	0xE000000

#define MEM_WINDOWS_SZ 	0x00020000 
#define eth1_region0 	0xF4200000
#define led_offset 		0x00E00
#define GPR_offset		0x4074		//good packet recieved register 
#define RCR_offset		0x100		//receive control Register 
#define enable_RCR		0x2			//value to enable receive control register 
/*
#define ALL_OFF			0xF0F0F0F
#define ALL_ON			0xE0E0E0E
*/
int main(int argc, char* argv[])
{
	uint32_t *led_addr, intial_led_addr, *good_packet_receievd_addr, *RCR;
	void *base_addr;
	//open memory map
	dev_mem_fd = open("/dev/mem", O_RDWR);
	if(dev_mem_fd == -1)
	{
		printf("Error opening dev_mem_fd\n");
		return -1;
	}
	//do the map to virtual space
	// NULL = whatever virtual address you want mapped to the physical address, 
		//NULL lets the kernel device what virtual address to give you 
	//mem_windows_sz = size of region 
	base_addr=mmap(NULL, MEM_WINDOWS_SZ, PROT_READ|PROT_WRITE,
		MAP_SHARED, dev_mem_fd, eth1_region0);
		printf("%p\n",(void *)&base_addr);

	//test file open
	if (base_addr == MAP_FAILED)
	{
		printf("mmap failed\n");
		return -1;
	}

	//create the correct led ctl address
	led_addr = (uint32_t*) (led_offset + base_addr);
	//save current value
	intial_led_addr = *led_addr;

	RCR = (uint32_t*) (RCR_offset + base_addr);
	*RCR = enable_RCR;


	//print led value 
	printf("led ctl address:%p\n",&led_addr);
	printf("led ctl vlaue: %x\n", *led_addr);
	//write to led vlue 
	printf("doing led writes\n");
	
	//turn LED0 and LED2 ON
	*led_addr = LED_ZERO_OFF | LED_ONE_ON | LED_TWO_OFF | LED_THREE_ON;
	printf("LED0 and LED2 ON\n");
	sleep(2);

	//turn all LEDs OFF
	*led_addr = LED_ZERO_OFF | LED_ONE_OFF | LED_TWO_OFF | LED_THREE_OFF;
	printf("LED0 and LED2 OFF\n");
	sleep(2);
	
	//loop 5 times and turn LED3, LED2, LED1, LED 0 on for 1 second each
	for (int i=0; i<5; i++)
	{
		printf("running loop %d\n",i+1);
		*led_addr = LED_THREE_ON;
		printf(".");
		fflush(stdout);
		sleep(1);
		*led_addr = LED_THREE_OFF | LED_TWO_ON;
		printf(".");
		fflush(stdout);
		sleep(1);
		*led_addr = LED_TWO_OFF | LED_ONE_ON;
		printf(".");
		fflush(stdout);
		sleep(1);
		*led_addr = LED_TWO_OFF | LED_ZERO_ON;
		printf(".");
		fflush(stdout);
		sleep(1);
		*led_addr = LED_ONE_OFF;
		printf(".");
		fflush(stdout);
		sleep(1);
		printf("\n");
	}
	
	//restore LEDCTL to intial value 
	*led_addr = intial_led_addr;
	printf("led ctl intial vlaue: %x\n", *led_addr);

	//read and write - good packet recieved statistics register 
	good_packet_receievd_addr = (uint32_t*) (GPR_offset + base_addr);
	printf("good packet recieved vlaue: %x\n", *good_packet_receievd_addr);

	//clean up and unmap 
	munmap(base_addr, MEM_WINDOWS_SZ);
	return 0;
}
