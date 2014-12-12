#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#ifdef CTL
#include <getopt.h>
#endif
#include "gpiolib.h"

void gpio_direction(int gpio, int dir)
{
	int ret = 0;
	char buf[50];
	sprintf(buf, "/sys/class/gpio/gpio%d/direction", gpio);
	int gpiofd = open(buf, O_WRONLY);
	assert(gpiofd != -1);

	if(dir == 1)
		write(gpiofd, "out", 3);
	else
		write(gpiofd, "in", 2);

	close(gpiofd);
}

int gpio_open(int gpio)
{
	int efd;
	efd = open("/sys/class/gpio/export", O_WRONLY);

	if(efd != -1) {
		int gpiofd, ret;
		char buf[50];
		sprintf(buf, "%d", gpio); 
		ret = write(efd, buf, strlen(buf));
		close(efd);
		if(ret == -1) return -1;
		sprintf(buf, "/sys/class/gpio/gpio%d/value", gpio);
		gpiofd = open(buf, O_WRONLY);
		return gpiofd;
	} else {
		return -1;
	}
}

void gpio_close(int gpio, int gpiofd)
{
	int fd;
	char buf[50];
	assert(gpiofd);
	close(gpiofd);

	fd = open("/sys/class/gpio/unexport", O_WRONLY);
	sprintf(buf, "%d", gpio);
	write(fd, buf, strlen(buf));
	close(fd);
}

int gpio_read(int gpiofd)
{
	char in[2];
	read(gpiofd, &in, 1);
	return atoi(in);
}

#ifdef CTL

static void usage(char **argv) {
	fprintf(stderr, "Usage: %s [OPTION] ...\n"
	  "Simple gpio access\n"
	  "\n"
	  "  -h, --help             This message\n"
	  "  -p, --getin <dio>      Returns the input value of n sysfs DIO\n"
	  "  -e, --setout <dio>     Sets a sysfs DIO output value high\n"
	  "  -l, --clrout <dio>     Sets a sysfs DIO output value low\n"
	  "  -d, --ddrout <dio>     Set sysfs DIO to an output\n"
	  "  -r, --ddrin <dio>      Set sysfs DIO to an input\n\n",
	  argv[0]
	);
}

int main(int argc, char **argv)
{
	int c;
	static struct option long_options[] = {
	  { "getin", 1, 0, 'p' },
	  { "setout", 1, 0, 'e' },
	  { "clrout", 1, 0, 'l' },
	  { "ddrout", 1, 0, 'd' },
	  { "ddrin", 1, 0, 'r' },
	  { "help", 0, 0, 'h' },
	  { 0, 0, 0, 0 }
	};

	if(argc == 1) {
		usage(argv);
		return(1);
	}

	while((c = getopt_long(argc, argv, "p:e:l:d:r:", long_options, NULL)) != -1) {
		int gpiofd;
		int gpio, i;

		switch(c) {
		case 'p':
			gpio = atoi(optarg);
			gpiofd = gpio_open(gpio);
			printf("gpio%d=%d\n", gpio, gpio_read(gpio));
			gpio_close(gpio, gpiofd);
			break;
		case 'e':
			gpio = atoi(optarg);
			gpiofd = gpio_open(gpio);
			write(gpiofd, "1", 1);
			gpio_close(gpio, gpiofd);
			break;
		case 'l':
			gpio = atoi(optarg);
			gpiofd = gpio_open(gpio);
			write(gpiofd, "1", 0);
			gpio_close(gpio, gpiofd);
			break;
		case 'd':
			gpio = atoi(optarg);
			gpiofd = gpio_open(gpio);
			gpio_direction(gpiofd, 1);
			gpio_close(gpio, gpiofd);
			break;
		case 'r':
			gpio = atoi(optarg);
			gpiofd = gpio_open(gpio);
			gpio_direction(gpiofd, 0);
			gpio_close(gpio, gpiofd);
			break;
		case 'h':
		default:
			usage(argv);
		}
	}
}

#endif // CTL