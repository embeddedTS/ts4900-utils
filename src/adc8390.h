#ifndef __ADC8390_H_
#define __ADC8390_H_

#include <stdint.h>

int adc_init();
int adc_readchannel(int twifd, int channel);

/* Convert raw samples to mV.
 * Absolute max 18.611V, can sense up to 10.301V. */
uint32_t scale_10v_inputs(uint16_t reg);

/* Convert raw samples to mV.  Max is 2.048V. */
uint32_t scale_diff_inputs(uint16_t reg);

#endif
