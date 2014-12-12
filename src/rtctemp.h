s#ifndef __RTCTEMP_H_
#define __RTCTEMP_H_

#include <stdint.h>

int rtctemp_init();
// Returns in millicelcius
int rtctemp_read(int twifd);

#endif
