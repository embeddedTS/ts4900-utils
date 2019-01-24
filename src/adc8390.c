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
			char path[512], busname[512];
			int namefd;
			snprintf(path, 512, "/sys/bus/i2c/devices/%s/name", dir->d_name);
			namefd = open(path, O_RDONLY);
			if(namefd == -1) continue;
			if(read(namefd, busname, 128) == -1) perror("busname");
			if(strstr(busname, "adc-i2c") != 0)
			{
				snprintf(path, 512, "/dev/%s", dir->d_name);
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

uint8_t mcp3428_conf(int twifd, uint8_t i2caddr, uint8_t cmd)
{
	struct i2c_rdwr_ioctl_data packets;
	struct i2c_msg msg;

	msg.addr = i2caddr;
	msg.flags = 0;
	msg.len	= 1;
	msg.buf	= (char *)&cmd;

	packets.msgs = &msg;
	packets.nmsgs = 1;

	if(ioctl(twifd, I2C_RDWR, &packets) < 0) {
		perror("Unable to send data");
		return 1;
	}
	return 0;
}

uint32_t mcp3428_sample(int twifd, uint8_t i2caddr)
{
	struct i2c_rdwr_ioctl_data packets;
	struct i2c_msg msg;
	uint32_t sample = 0;

	msg.addr = i2caddr;
	msg.flags = I2C_M_RD;
	msg.len	= 3;
	msg.buf	= (char *)&sample;

	packets.msgs = &msg;
	packets.nmsgs = 1;

	if(ioctl(twifd, I2C_RDWR, &packets) < 0) {
		perror("Unable to send data");
		return 0;
	}

	if(!(sample & 0x800000))
		return 0xffffffff;

	// mask out conf bits
	sample &= 0xffff;

	// Return byte swapped
	return (sample>>8) | (sample<<8);
}

uint16_t adc_readchannel(int twifd, int channel)
{
	uint32_t data;
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
	lseek(anselfd, 0, SEEK_SET);

	mcp3428_conf(twifd, 0x68, 0x78 | (chan << 5));

	usleep(95000);
	do {
		data = mcp3428_sample(twifd, 0x68);
	} while (data == 0xffffffff);

	return data;
}

// Absolute max 18.611V, can sense up to 10.301V.
uint32_t scale_10v_inputs(uint16_t reg)
{
	uint32_t val;
	/* fractions $((8060+2000)) 2000 4096 65536 */
	val = ((uint32_t)reg * 503)/1600;
	return val;
}

// scales to 2.048v in mv
uint32_t scale_diff_inputs(uint16_t reg)
{
	uint32_t val;
	/* fractions 4096 65536 */
	val = reg/16;
	return val;
}

#ifdef CTL
int main(int argc, char **argv)
{
	int twifd, c;
	int addr = -1;
	uint8_t data;
	int temp;

	twifd = adc_init();
	if(twifd == -1){
		fprintf(stderr, "Cannot find the ADC device\n");
		return 1;
	}

	printf("adc0_mv=%d\n", scale_diff_inputs(adc_readchannel(twifd, 0)));
	printf("adc1_mv=%d\n", scale_diff_inputs(adc_readchannel(twifd, 1)));
	printf("adc2_mv=%d\n", scale_10v_inputs(adc_readchannel(twifd, 2)));
	printf("adc3_mv=%d\n", scale_10v_inputs(adc_readchannel(twifd, 3)));
	printf("adc4_mv=%d\n", scale_10v_inputs(adc_readchannel(twifd, 4)));
	printf("adc5_mv=%d\n", scale_10v_inputs(adc_readchannel(twifd, 5)));

	close(twifd);
	return 0;
}
#endif
