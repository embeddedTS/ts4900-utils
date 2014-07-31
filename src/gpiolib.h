#ifndef _GPIOLIB_H_

// returns -1 or the file descriptor of the gpio value file
int gpio_open(int gpio);
// 1 output, 0 input
void gpio_direction(int gpio, int dir);
void gpio_close(int gpiofd);
int gpio_read(int gpiofd);

#endif //_GPIOLIB_H_