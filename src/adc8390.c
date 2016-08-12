#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h> 
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <string.h>

#ifdef CTL
#include <getopt.h>
#endif
#include "i2c-dev.h"

int anselfd;
#define ANSEL_GPIO 	74
#define CHAN1		0x98
#define CHAN2		0xb8
#define CHAN3		0xd8
#define CHAN4		0xf8

int adc_init()
{
	char path[256];
	int fd = -1;
	int len;
	DIR *d;
	struct dirent *dir;
	int ret;

	memset(path, 0, 256);
	anselfd = open("/sys/class/gpio/export", O_WRONLY | O_SYNC);
	if(anselfd == -1) perror("adc_init 1");
	len = snprintf(path, 255, "%d", ANSEL_GPIO);
	ret = write(anselfd, path, len);
	close(anselfd);
	snprintf(path, 255, "/sys/class/gpio/gpio%d/direction", ANSEL_GPIO);
	anselfd = open(path, O_RDWR | O_SYNC);
	if(anselfd == -1) perror("adc_init 2");
	ret = write(anselfd, "out", 3);
	close(anselfd);
	snprintf(path, 255, "/sys/class/gpio/gpio%d/value", ANSEL_GPIO);
	anselfd = open(path, O_RDWR | O_SYNC);
	if(anselfd == -1) perror("adc_init 3");

	// Depending on which baseboard the TS-4900 is used on there
	// May be a different number of /dev/i2c-* devices.  This will 
	// search for the name adc-i2c.29 where the name is the 
	// memory address in the imx6 of the correct i2c bus.
	d = opendir("/sys/bus/i2c/devices/");
	if (d){
		while ((dir = readdir(d)) != NULL) {
			char path[128], busname[128];
			int namefd;
			snprintf(path, 100, "/sys/bus/i2c/devices/%s/name", dir->d_name);
			namefd = open(path, O_RDONLY);
			if(namefd == -1) continue;
			if(read(namefd, busname, 128) == -1) perror("busname");
			if(strstr(busname, "adc-i2c") != 0)
			{
				snprintf(path, 100, "/dev/%s", dir->d_name);
				fd = open(path, O_RDWR);
			}
		}
		closedir(d);
	}

	if(fd != -1) {
		if (ioctl(fd, I2C_SLAVE, 0x68) < 0) {
			perror("ADC did not ACK 0x68\n");
			return -1;
		}
	}

	return fd;
}

int adc_readchannel(int twifd, int channel)
{
	uint8_t data[3];
	int chan = 0, ret = 0;

	switch (channel)
	{
	case 0:
	case 1:
		chan = channel;
		break;
	case 2:
		chan = 2;
		ret = write(anselfd, "0", 1);
		break;
	case 3:
		chan = 2;
		ret = write(anselfd, "1", 1);
		break;
	case 4:
		chan = 3;
		ret = write(anselfd, "0", 1);
		break;
	case 5:
		chan = 3;
		ret = write(anselfd, "1", 1);
		break;
	}

	ioctl(twifd, I2C_SLAVE, 0x68);
	i2c_smbus_write_byte(twifd, 0x98 | (chan << 5));
	usleep(95000);
	do {
		i2c_smbus_read_i2c_block_data(twifd, 0x98 | 
			(chan << 5), 3, data);
	} while ((data[2] & 0x80) == 0x80);
	return data[1] | (data[0] << 8);
}

float tov(int raw)
{
	float mv = raw * 0.000316;
	return mv;
}

#ifdef CTL

static void usage(char **argv) {
	fprintf(stderr, "Usage: %s [OPTION] ...\n"
	  "Technologic Systems TS-8390 ADC\n"
	  "\n"
	  "  -h, --help     This message\n"
	  "  -r, --read     Read all ADCs once\n"
	  "  -s, --showraw  Show raw instead of volts\n"
	  "  	Note: Differential channels always print raw values\n",
	  	argv[0]
	);
}

int main(int argc, char **argv)
{
	int twifd, c;
	int addr = -1;
	uint8_t data;
	int temp;
	int opt_speed = 15;
	int opt_showraw = 0;
	static struct option long_options[] = {
	  { "read", 0, 0, 'r' },
	  { "showraw", 1, 0, 's' },
	  { "help", 0, 0, 'h' },
	  { 0, 0, 0, 0 }
	};

	twifd = adc_init();
	if(twifd == -1){
		fprintf(stderr, "Cannot find the ADC device\n");
		return 1;
	}

	while((c = getopt_long(argc, argv, "hrs", long_options, NULL)) != -1) {
		switch (c) {
		case 's':
			opt_showraw = 1;
			break;
		case 'r':
			if(opt_showraw) {
				printf("adc0=%d\n", adc_readchannel(twifd, 0));
				printf("adc1=%d\n", adc_readchannel(twifd, 1));
				printf("adc2=%d\n", adc_readchannel(twifd, 2));
				printf("adc3=%d\n", adc_readchannel(twifd, 3));
				printf("adc4=%d\n", adc_readchannel(twifd, 4));
				printf("adc5=%d\n", adc_readchannel(twifd, 5));
			} else {
				printf("adc0=%d\n", adc_readchannel(twifd, 0));
				printf("adc1=%d\n", adc_readchannel(twifd, 1));
				printf("adc2=%.4f\n", tov(adc_readchannel(twifd, 2)));
				printf("adc3=%.4f\n", tov(adc_readchannel(twifd, 3)));
				printf("adc4=%.4f\n", tov(adc_readchannel(twifd, 4)));
				printf("adc5=%.4f\n", tov(adc_readchannel(twifd, 5)));
			}

			break;
		case 'h':
		default:
			usage(argv);
			return 1;
		}
	}

	close(twifd);
	return 0;
}
#endif
