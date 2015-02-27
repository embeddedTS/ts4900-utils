#ifndef _GPIOLIB_H_

// returns -1 or the file descriptor of the gpio value file
int gpio_open(int gpio);
// 1 output, 0 input
int gpio_direction(int gpio, int dir);
void gpio_unexport(int gpio);
int gpio_read(int gpio);
int gpio_write(int gpio, int val);
int gpio_setedge(int gpio, int rising, int falling);
int gpio_select(int gpio);

#endif //_GPIOLIB_H_