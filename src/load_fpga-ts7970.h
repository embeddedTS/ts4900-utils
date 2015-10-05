#ifndef _LOAD_FPGA_TS7970_
#define _LOAD_FPGA_TS7970_

#include <stdint.h>

void init_ts7970(void);
void restore_ts7970(void);
int readport_ts7970(void);
void writeport_ts7970(int pins, int val);
void sclock_ts7970(void);

#endif