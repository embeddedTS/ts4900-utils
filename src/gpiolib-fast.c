#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "gpiolib-fast.h"

volatile uint32_t* gpiofast_init()
{
	int mem;
	volatile uint32_t *gpioreg;

	mem = open("/dev/mem", O_RDWR|O_SYNC);
	if (mem < 1){ 
		perror("Couldn't open /dev/mem");
		return 0;
	}
	gpioreg = mmap(0, getpagesize() * 28, PROT_READ|PROT_WRITE, MAP_SHARED, mem, MX6_GPIO_BASE);
	if(gpioreg == 0) {
		perror("Couldn't map GPIO mem");
		return 0;
	}
	return gpioreg;
}
