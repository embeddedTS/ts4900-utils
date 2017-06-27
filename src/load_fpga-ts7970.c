#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "gpiolib-fast.h"
#include "ispvm.h"

volatile uint32_t *mx6gpio;
static unsigned int gpiostate;

#define TDI 1<<16
#define TCK 1<<11
#define TMS 1<<8
#define TDO 1<<12

void init_ts7970(void)
{
	mx6gpio = gpiofast_init();
	assert(mx6gpio != 0);

	mx6gpio[(MX6_GPIO_BANK5 + GPGDIR)/4] |= TDI  | TMS | TCK;
	mx6gpio[(MX6_GPIO_BANK5 + GPGDIR)/4] &= ~(TDO);

	mx6gpio[(MX6_GPIO_BANK5 + GPDR)/4] &= ~(TCK);
	gpiostate = mx6gpio[(MX6_GPIO_BANK5 + GPDR)/4];
}

void restore_ts7970(void)
{
	mx6gpio[(MX6_GPIO_BANK5 + GPGDIR)/4] &= ~(TDI | TCK | TMS);
}

int readport_ts7970(void)
{
	return (mx6gpio[(MX6_GPIO_BANK5 + GPPSR)/4] & TDO) ? 1 : 0;
}

void writeport_ts7970(int pins, int val)
{
	gpiostate = mx6gpio[(MX6_GPIO_BANK5 + GPDR)/4];

	if(val) {
		if(pins & g_ucPinTDI)
			gpiostate |= TDI;
		if(pins & g_ucPinTCK)
			gpiostate |= TCK;
		if(pins & g_ucPinTMS)
			gpiostate |= TMS;
	} else {
		if(pins & g_ucPinTDI)
			gpiostate &= ~TDI;
		if(pins & g_ucPinTCK)
			gpiostate &= ~TCK;
		if(pins & g_ucPinTMS)
			gpiostate &= ~TMS;
	}

	mx6gpio[(MX6_GPIO_BANK5 + GPDR)/4] = gpiostate;
}

void sclock_ts7970()
{
	assert((gpiostate & TCK) == 0);
	mx6gpio[(MX6_GPIO_BANK5 + GPDR)/4] |= TCK;
	mx6gpio[(MX6_GPIO_BANK5 + GPDR)/4] &= ~(TCK);
}
