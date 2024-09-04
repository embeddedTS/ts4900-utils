#ifndef __FPGA_H_
#define __FPGA_H_

struct cbarpin
{
	int addr;
	char *name;
};

int fpga_init(void);
void fpoke8(int i2cfd, uint16_t addr, uint8_t value);
uint8_t fpeek8(int i2cfd, uint16_t addr);

#endif