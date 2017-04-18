#include <stdio.h>
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

void rtc_enabletemp(int twifd)
{
	uint8_t addr = 0x0d;
	uint8_t data;

	i2c_smbus_write_byte(twifd, 0x0d);
	data = i2c_smbus_read_byte(twifd);
	if (data & (1 << 7) != (1 << 7)) {
		i2c_smbus_write_byte(twifd, 0x0d);
		// Set TSE bit
		data |= (1 << 7);
		i2c_smbus_write_byte(twifd, data);
	}
}

int rtctemp_init()
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
		if (ioctl(fd, I2C_SLAVE_FORCE, 0x6f) < 0) {
			perror("FPGA did not ACK 0x6f\n");
			return -1;
		}
	}

	// Enable temp sensor
	rtc_enabletemp(fd);

	return fd;
}

int rtctemp_read(int twifd)
{
	uint8_t data[2];

	i2c_smbus_write_byte(twifd, 0x28);
	data[0] = i2c_smbus_read_byte(twifd);
	data[1] = i2c_smbus_read_byte(twifd);

	return ((data[0]|(data[1]<<8))*500)-273000;
}

#ifdef CTL

static void usage(char **argv) {
	fprintf(stderr, "Usage: %s [OPTION] ...\n"
	  "Technologic Systems RTC tempsensor access\n"
	  "\n"
	  "  -h, --help            This message\n"
	  "  -g, --gettemp         Read the RTC temperature\n",
	  argv[0]
	);
}

int main(int argc, char **argv)
{
	int twifd, c;
	int addr = -1;
	uint8_t data;
	int temp;
	static struct option long_options[] = {
	  { "gettemp", 0, 0, 'g' },
	  { "help", 0, 0, 'h' },
	  { 0, 0, 0, 0 }
	};

	if(argc == 1) {
		usage(argv);
		return(1);
	}
	twifd = rtctemp_init();
	if(twifd == -1)
		return 1;

	while((c = getopt_long(argc, argv, "hg", long_options, NULL)) != -1) {
		switch (c) {
		case 'g':
			temp = rtctemp_read(twifd);
			printf("rtctemp_millicelcius=%d\n", temp);
			break;
		case 'h':
		default:
			usage(argv);
		}
	}

	cleanup:
	close(twifd);
	return 0;

	failed:
	close(twifd);
	return 1;
}
#endif
