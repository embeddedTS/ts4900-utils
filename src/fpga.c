#include <stdio.h>
#include <unistd.h>
#include <dirent.h> 
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdint.h>

#include "i2c-dev.h"

int fpga_init()
{
	static int fd = -1;
	DIR *d;
	struct dirent *dir;

	if(fd != -1)
		return fd;

	// Depending on which baseboard the TS-4900 is used on there
	// May be a different number of /dev/i2c-* devices.  This will 
	// search for the name 21a0000.i2c where the name is the 
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
			if(strncmp(busname, "21a0000.i2c", 11) == 0)
			{
				snprintf(path, 100, "/dev/%s", dir->d_name);
				fd = open(path, O_RDWR);
			}
		}
		closedir(d);
	}

	if(fd != -1) {
		if (ioctl(fd, I2C_SLAVE_FORCE, 0x28) < 0) {
			perror("FPGA did not ACK 0x28\n");
			return -1;
		}
	}

	return fd;
}

void fpoke8(int twifd, uint16_t addr, uint8_t value)
{
	uint8_t data[3];
	data[0] = ((addr >> 8) & 0xff);
	data[1] = (addr & 0xff);
	data[2] = value;
	if (write(twifd, data, 3) != 3) {
		perror("I2C Write Failed");
	}
}

uint8_t fpeek8(int twifd, uint16_t addr)
{
	uint8_t data[2];
	data[0] = ((addr >> 8) & 0xff);
	data[1] = (addr & 0xff);
	if (write(twifd, data, 2) != 2) {
		perror("I2C Address set Failed");
	}
	read(twifd, data, 1);

	return data[0];
}
