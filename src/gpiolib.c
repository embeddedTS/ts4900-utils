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

int gpio_export(int gpio)
{
	int efd;
	char buf[50];
	int gpiofd, ret;
	efd = open("/sys/class/gpio/export", O_WRONLY);

	if(efd != -1) {
		sprintf(buf, "%d", gpio); 
		ret = write(efd, buf, strlen(buf));
		if(ret < 0) {
			perror("Export failed");
			return -2;
		}
		close(efd);
	} else {
		// If we can't open the export file, we probably
		// dont have any gpio permissions
		return -1;
	}
	return 0;
}

void gpio_unexport(int gpio)
{
	int gpiofd;
	char buf[50];
	gpiofd = open("/sys/class/gpio/unexport", O_WRONLY);
	sprintf(buf, "%d", gpio);
	write(gpiofd, buf, strlen(buf));
	close(gpiofd);
}

int gpio_read(int gpio)
{
	char in[3] = {0, 0, 0};
	char buf[50];
	int nread, gpiofd;
	sprintf(buf, "/sys/class/gpio/gpio%d/value", gpio);
	gpiofd = open(buf, O_RDWR);
	if(gpiofd < 0) {
		fprintf(stderr, "Failed to open gpio %d value\n", gpio);
		perror("gpio failed");
	}

	do {
		nread = read(gpiofd, in, 1);
	} while (nread == 0);
	if(nread == -1){
		perror("GPIO Read failed");
		return -1;
	}
	
	close(gpiofd);
	return atoi(in);
}

int gpio_write(int gpio, int val)
{	
	char buf[50];
	int nread, ret, gpiofd;
	sprintf(buf, "/sys/class/gpio/gpio%d/value", gpio);
	gpiofd = open(buf, O_RDWR);
	if(gpiofd > 0) {
		snprintf(buf, 2, "%d", val);
		ret = write(gpiofd, buf, 2);
		if(ret < 0) {
			perror("failed to set gpio");
			return 1;
		}

		close(gpiofd);
		if(ret == 2) return 0;
	}
	return 1;
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
		int gpio, i;

		switch(c) {
		case 'p':
			gpio = atoi(optarg);
			gpio_export(gpio);
			printf("gpio%d=%d\n", gpio, gpio_read(gpio));
			gpio_unexport(gpio);
			break;
		case 'e':
			gpio = atoi(optarg);
			gpio_export(gpio);
			gpio_write(gpio, 1);
			gpio_unexport(gpio);
			break;
		case 'l':
			gpio = atoi(optarg);
			gpio_export(gpio);
			gpio_write(gpio, 0);
			gpio_unexport(gpio);
			break;
		case 'd':
			gpio = atoi(optarg);
			gpio_export(gpio);
			gpio_direction(gpio, 1);
			gpio_unexport(gpio);
			break;
		case 'r':
			gpio = atoi(optarg);
			gpio_export(gpio);
			gpio_direction(gpio, 0);
			gpio_unexport(gpio);
			break;
		case 'h':
		default:
			usage(argv);
		}
	}
}

#endif // CTL