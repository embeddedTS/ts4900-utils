#ifndef __RTCNVRAM_H_
#define __RTCNVRAM_H_

#include <stdint.h>

int nvram_init();
int nvram_poke8(int twifd, uint8_t addr, uint8_t value);
uint8_t nvram_peek8(int twifd, uint8_t addr);

#endif
