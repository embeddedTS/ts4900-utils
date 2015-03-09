#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "gpiolib-fast.h"

volatile uint32_t *gpioreg;

int gpiofast_direction(int gpio, int dir)
{
	int bank = gpio / 32;
	int io = gpio % 32;

	if(dir)	gpioreg[((bank * 0x4000) + 0x4) / 4] |= (1 << io);
	else gpioreg[((bank * 0x4000) + 0x4) / 4] &= ~(1 << io);
}

int gpiofast_read(int gpio)
{
	int bank = gpio / 32;
	int io = gpio % 32;

	return gpioreg[((bank * 0x4000) + 0x8) / 4] & (1 << io) ? 1 : 0;
}

int gpiofast_write(int gpio, int val)
{
	int bank = gpio / 32;
	int io = gpio % 32;

	if(val)	gpioreg[(bank * 0x4000) / 4] |= (1 << io);
	else gpioreg[(bank * 0x4000) / 4] &= ~(1 << io);
}

int gpiofast_init()
{
	int mem;
	mem = open("/dev/mem", O_RDWR|O_SYNC);
	if (mem < 1){ 
		perror("Couldn't open /dev/mem");
		return -1;
	}
	gpioreg = mmap(0, getpagesize() * 20, PROT_READ|PROT_WRITE, MAP_SHARED, mem, 0x0209C000);
	if(gpioreg == 0) {
		perror("Couldn't map GPIO mem");
		return -2;
	}
}
