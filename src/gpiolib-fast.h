#ifndef _GPIOLIB_H_

/* This interface accesses the GPIO with a direct memory map which is much faster
 * than the sysfs gpio mechanism.  For example, toggling a single DIO
 * Square wave with sysfs: 150khz
 * Square wave with direct regs: 1.66MHz
 *
 * These expect the same GPIO number as Linux.
 */

// returns -1 or the file descriptor of the gpio value file
int gpiofast_init();
// 1 output, 0 input
int gpiofast_direction(int gpio, int dir);
int gpiofast_read(int gpio);
int gpiofast_write(int gpio, int val);

#endif //_GPIOLIB_H_