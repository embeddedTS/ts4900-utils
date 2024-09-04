#include <stdio.h>
#include <unistd.h>
#include <dirent.h> 
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "i2c-dev.h"

#define FPGA_ADDR 0x28

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
			char path[512], busname[512];
			int namefd;
			snprintf(path, 512, "/sys/bus/i2c/devices/%s/name", dir->d_name);
			namefd = open(path, O_RDONLY);
			if(namefd == -1) continue;
			if(read(namefd, busname, 512) == -1) perror("busname");
			if(strncmp(busname, "21a0000.i2c", 11) == 0)
			{
				snprintf(path, 512, "/dev/%s", dir->d_name);
				fd = open(path, O_RDWR);
			}
		}
		closedir(d);
	}

	if(fd != -1) {
		if (ioctl(fd, I2C_SLAVE_FORCE, FPGA_ADDR) < 0) {
			perror("FPGA did not ACK\n");
			return -1;
		}
	}

	return fd;
}


int fpeekstream8(int twifd, uint8_t *data, uint16_t addr, int size)
{
	struct i2c_rdwr_ioctl_data packets;
	struct i2c_msg msgs[2];
	char busaddr[2];

	/* Linux only supports 4k transactions at a time, and we need
	 * two bytes for the address */
	assert(size <= 4094);

	busaddr[0]    = ((addr >> 8) & 0xff);
	busaddr[1]    = (addr & 0xff);

	msgs[0].addr  = FPGA_ADDR;
	msgs[0].flags = 0;
	msgs[0].len   = 2;
	msgs[0].buf   = busaddr;

	msgs[1].addr  = FPGA_ADDR;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len   = size;
	msgs[1].buf   = (char *)data;

	packets.msgs = msgs;
	packets.nmsgs = 2;

	if(ioctl(twifd, I2C_RDWR, &packets) < 0) {
		perror("Unable to read I2C data");
		return 1;
	}
	return 0;
}

int fpokestream8(int twifd, uint8_t *data, uint16_t addr, int size)
{
	struct i2c_rdwr_ioctl_data packets;
	struct i2c_msg msg;
	uint8_t outdata[4096];

	/* Linux only supports 4k transactions at a time, and we need
	 * two bytes for the address */
	assert(size <= 4094);

	outdata[0] = ((addr >> 8) & 0xff);
	outdata[1] = (addr & 0xff);
	memcpy(&outdata[2], data, size);

	msg.addr   = FPGA_ADDR;
	msg.flags  = 0;
	msg.len	   = 2 + size;
	msg.buf	   = (char *)outdata;

	packets.msgs = &msg;
	packets.nmsgs = 1;

	if(ioctl(twifd, I2C_RDWR, &packets) < 0) {
		perror("Unable to send I2C data");
		return 1;
	}
	return 0;
}

void fpoke8(int twifd, uint16_t addr, uint8_t data)
{
	int ret;

	ret = fpokestream8(twifd, &data, addr, 1);
	if (ret) {
		perror("Failed to write to FPGA");
	}
}

uint8_t fpeek8(int twifd, uint16_t addr)
{
	uint8_t data = 0;
	int ret;
	ret = fpeekstream8(twifd, &data, addr, 1);

	if (ret) {
		perror("Failed to read from FPGA");
	}
	return data;
}
