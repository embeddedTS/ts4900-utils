#ifndef __FPGA_H_
#define __FPGA_H_

int fpga_init();
void fpoke16(int twifd, uint16_t addr, uint8_t value);
uint8_t fpeek16(int twifd, uint16_t addr);

#endif