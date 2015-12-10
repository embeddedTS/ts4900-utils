#ifndef _LOAD_FPGA_TS7990_
#define _LOAD_FPGA_TS7990_

#include <stdint.h>

void init_ts7990(void);
void restore_ts7990(void);
int readport_ts7990(void);
void writeport_ts7990(int pins, int val);
void sclock_ts7990(void);

#endif