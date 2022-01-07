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

int nvram_init()
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
		if (ioctl(fd, I2C_SLAVE, 0x57) < 0) {
			perror("FPGA did not ACK 0x57\n");
			return -1;
		}
	}

	return fd;
}

int nvram_poke8(int twifd, uint8_t addr, uint8_t value)
{
	uint8_t data[2];

	data[0] = addr;
	data[1] = value;
	if (write(twifd, data, 2) != 2) {
		perror("nvram write failed");
		return 1;
	}
	return 0;
}

uint8_t nvram_peek8(int twifd, uint8_t addr)
{
	uint8_t data = 0;

	if (write(twifd, &addr, 1) != 1) {
		perror("I2C Address set Failed");
	}
	if(read(twifd, &data, 1) != 1)
		perror("nvram read failed");

	return data;
}

#ifdef CTL

static void usage(char **argv) {
	fprintf(stderr, "Usage: %s [OPTION] ...\n"
	  "embeddedTS RTC nvram access\n"
	  "\n"
	  "  -h, --help            This message\n"
	  "  -a, --addr=<address>  Set the NVRAM address (0-127)\n"
	  "  -g, --get             Get the specified address\n"
	  "  -s, --set=<value>     Set the specified address to value\n\n",
	  argv[0]
	);
}

int main(int argc, char **argv)
{
	int twifd, c;
	int addr = -1;
	uint8_t data;
	static struct option long_options[] = {
	  { "addr", 1, 0, 'a' },
	  { "get", 0, 0, 'g' },
	  { "set", 1, 0, 's' },
	  { "help", 0, 0, 'h' },
	  { 0, 0, 0, 0 }
	};

	if(argc == 1) {
		usage(argv);
		return(1);
	}
	twifd = nvram_init();
	if(twifd == -1)
		return 1;

	while((c = getopt_long(argc, argv, "ha:gs:", long_options, NULL)) != -1) {
		switch (c) {
		case 'a':
			addr = (uint8_t)strtoull(optarg, NULL, 0);
			if(addr > 127 || addr < 0){
				fprintf(stderr, "Address is out of bounds.  Must be in 0-127.\n");
				goto failed;
			}
			break;
		case 'g':
			if(addr == -1) {
				fprintf(stderr, "Address must be passed first\n");
				return 1;
			}
			data = nvram_peek8(twifd, addr);
			printf("nvram%d=0x%X\n", addr, data);
			break;
		case 's':
			if(addr == -1) {
				fprintf(stderr, "Address must be passed first\n");
				return 1;
			}
			data = (uint8_t)strtoull(optarg, NULL, 0);
			nvram_poke8(twifd, addr, data);
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
