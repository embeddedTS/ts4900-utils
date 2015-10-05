#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "gpiolib-fast.h"

volatile uint32_t *mx6gpio;
static unsigned int gpiostate;

static const unsigned int g_ucPinTDI     = 1<<16;   /* Bit address of TDI */
static const unsigned int g_ucPinTCK     = 1<<11;   /* Bit address of TCK */
static const unsigned int g_ucPinTMS     = 1<<8;   /* Bit address of TMS */
static const unsigned int g_ucPinENABLE  = 0;       /* Bit address of ENABLE */
static const unsigned int g_ucPinTRST    = 0;       /* Bit address of TRST */
static const unsigned int g_ucPinTDO     = 1<<12;   /* Bit address of TDO*/

void init_ts7970(void)
{
	mx6gpio = gpiofast_init();
	assert(mx6gpio != 0);
	mx6gpio[(MX6_GPIO_BANK5 + GPGDIR)/4] |= g_ucPinTDI | g_ucPinTCK | g_ucPinTMS;
	mx6gpio[(MX6_GPIO_BANK5 + GPGDIR)/4] &= ~g_ucPinTDO;
	gpiostate = mx6gpio[(MX6_GPIO_BANK5 + GPDR)/4];
}

void restore_ts7970(void)
{
	mx6gpio[(MX6_GPIO_BANK5 + GPGDIR)/4] &= ~(g_ucPinTDI | g_ucPinTCK | g_ucPinTMS);
}

int readport_ts7970(void)
{
	return (mx6gpio[(MX6_GPIO_BANK5 + GPPSR)/4] & g_ucPinTDO) ? 1 : 0;
}

void writeport_ts7970(int pins, int val)
{
	if (val) gpiostate |= pins;
	else gpiostate &= ~pins;

	mx6gpio[(MX6_GPIO_BANK5 + GPDR)/4] = gpiostate;
}

void sclock_ts7970()
{
	assert((gpiostate & g_ucPinTCK) == 0);
	mx6gpio[(MX6_GPIO_BANK5 + GPDR)/4] = g_ucPinTCK;
	mx6gpio[(MX6_GPIO_BANK5 + GPDR)/4] = ~(g_ucPinTCK);
}
