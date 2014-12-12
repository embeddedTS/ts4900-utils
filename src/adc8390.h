#ifndef __ADC8390_H_
#define __ADC8390_H_

#include <stdint.h>

int adc_init();
int adc_readchannel(int twifd, int channel);
float tov(int raw);

#endif
