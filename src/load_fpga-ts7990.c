#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "gpiolib.h"
#include "ispvm.h"

#define TS7970_JTAG_TMS 5
#define TS7970_JTAG_TCK 203
#define TS7970_JTAG_TDO 147
#define TS7970_JTAG_TDI 204

void init_ts7990(void)
{
	assert(gpio_export(TS7970_JTAG_TMS) == 0);
	assert(gpio_export(TS7970_JTAG_TCK) == 0);
	assert(gpio_export(TS7970_JTAG_TDO) == 0);
	assert(gpio_export(TS7970_JTAG_TDI) == 0);

	tmsfd = gpio_getfd(TS7970_JTAG_TMS);
	tckfd = gpio_getfd(TS7970_JTAG_TCK);
	tdofd = gpio_getfd(TS7970_JTAG_TDO);
	tdifd = gpio_getfd(TS7970_JTAG_TDI);

	assert(tmsfd >= 0);
	assert(tckfd >= 0);
	assert(tdofd >= 0);
	assert(tdifd >= 0);

	gpio_direction(TS7970_JTAG_TCK, 2);
	gpio_direction(TS7970_JTAG_TDI, 2);
	gpio_direction(TS7970_JTAG_TMS, 2);
	gpio_direction(TS7970_JTAG_TDO, 0);
}

void restore_ts7990(void)
{
	gpio_unexport(TS7970_JTAG_TMS);
	gpio_unexport(TS7970_JTAG_TDI);
	gpio_unexport(TS7970_JTAG_TCK);
	gpio_unexport(TS7970_JTAG_TDO);
}

int readport_ts7990(void)
{
	//return gpio_read(TS7970_JTAG_TDO);
	char in;
	read(tdofd, &in, 1);
	lseek(tdofd, 0, SEEK_SET);
	if(in == '1') return 1;
	return 0;
}

void writeport_ts7990(int pins, int val)
{
	uint8_t *buf;
	if(val) buf = "1";
	else buf = "0";

	switch (pins) {
	case g_ucPinTDI:
		write(tdifd, buf, 1);
		break;
	case g_ucPinTCK:
		write(tckfd, buf, 1);
		break;
	case g_ucPinTMS:
		write(tmsfd, buf, 1);
		break;
	default:
		printf("%s: requested unknown pin\n", __func__);
	}
}

void sclock_ts7990()
{
	write(tckfd, "1", 1);
	write(tckfd, "0", 1);
}
