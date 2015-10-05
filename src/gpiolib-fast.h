#ifndef _GPIOLIB_H_

#define MX6_GPIO_BASE 	0x0209C000
#define MX6_GPIO_BANK1	0x0
#define MX6_GPIO_BANK2	0x4000
#define MX6_GPIO_BANK3	0x8000
#define MX6_GPIO_BANK4	0xc000
#define MX6_GPIO_BANK5	0x10000
#define MX6_GPIO_BANK6	0x14000
#define MX6_GPIO_BANK7	0x18000

#define GPDR		0x0
#define GPGDIR		0x4
#define GPPSR		0x8

// returns 0 or the file descriptor of the gpio value file
volatile uint32_t* gpiofast_init();

#endif //_GPIOLIB_H_