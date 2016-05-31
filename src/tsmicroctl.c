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

char *model = 0;

char *get_model()
{
	FILE *proc;
	char mdl[256];
	char *ptr;
	int sz;

	proc = fopen("/proc/device-tree/model", "r");
	if (!proc) {
	    perror("model");
	    return 0;
	}
	sz = fread(mdl, 256, 1, proc);
	ptr = strstr(mdl, "TS-");
	return strndup(ptr, sz - (mdl - ptr));
}

int silabs_init()
{
	static int fd = -1;
	fd = open("/dev/i2c-0", O_RDWR);
	if(fd != -1) {
		if (ioctl(fd, I2C_SLAVE_FORCE, 0x10) < 0) {
			perror("FPGA did not ACK 0x10\n");
			return -1;
		}
	}

	return fd;
}

// Scale voltage to silabs 0-2.5V
uint16_t inline sscale(uint16_t data){
	return data * 2.5 * 1024/1000;
}

// Scale voltage for resistor dividers
uint16_t inline rscale(uint16_t data, uint16_t r1, uint16_t r2)
{
	uint16_t ret = (data * (r1 + r2)/r2);
	return sscale(ret);
}

void do_sleep(int twifd, int seconds)
{
	unsigned char dat[4] = {0};
	int opt_sleepmode = 1; // Legacy mode on new boards
	int opt_resetswitchwkup = 1;

	dat[0]=(0x1 | (opt_resetswitchwkup << 1) |
	  ((opt_sleepmode-1) << 4) | 1 << 6);
	dat[3] = (seconds & 0xff);
	dat[2] = ((seconds >> 8) & 0xff);
	dat[1] = ((seconds >> 16) & 0xff);
	write(twifd, &dat, 4);
}

void do_info(int twifd)
{
	uint16_t data[16];
	uint8_t tmp[32];
	int i, ret;

	ret = read(twifd, tmp, 32);

	if(ret != 32){
		printf("I2C Read failed with %d\n", ret);
		return;
	}
    for (i = 0; i <= 15; i++)
    	data[i] = (tmp[i*2] << 8) | tmp[(i*2)+1];

	if(strstr(model, "7970")) {
		printf("VDD_ARM_CAP=%d\n", sscale(data[0]));
		printf("VDD_HIGH_CAP=%d\n", sscale(data[1]));
		printf("VDD_SOC_CAP=%d\n",sscale(data[2]));
		printf("VDD_ARM=%d\n", sscale(data[3]));
		printf("SILAB_P10=0x%X\n", data[4]);
		printf("SILAB_P11=0x%X\n", data[5]);
		printf("SILAB_P12=0x%X\n", data[6]);
		printf("VIN=%d\n", rscale(data[7], 2870, 147));
		printf("V5_A=%d\n", rscale(data[8], 147, 107));
		printf("V3P1=%d\n", rscale(data[9], 499, 499));
		printf("DDR_1P5V=%d\n", sscale(data[10]));
		printf("V1P8=%d\n", sscale(data[11]));
		printf("V1P2=%d\n", sscale(data[12]));
		printf("RAM_VREF=%d\n", sscale(data[13]));
		printf("V3P3=%d\n", rscale(data[14], 499, 499));
		printf("SILABREV=%d\n", data[15]);
	}
}

#ifdef CTL

static void usage(char **argv) {
	fprintf(stderr, "Usage: %s [OPTION] ...\n"
	  "Technologic Systems Microcontroller Access\n"
	  "\n"
	  "  -h, --help            This message\n"
	  "  -i, --info            Read all Silabs ADC values and rev\n"
	  "  -s, --sleep <seconds> Put the board in a sleep mode for n seconds\n"
	  "    All values are returned in mV except the P10 which are raw 12-bit values.\n\n",
	  argv[0]
	);
}

int main(int argc, char **argv)
{
	int c;
	int twifd;

	static struct option long_options[] = {
	  { "info", 0, 0, 'i' },
	  { "sleep", 1, 0, 's' },
	  { "help", 0, 0, 'h' },
	  { 0, 0, 0, 0 }
	};

	if(argc == 1) {
		usage(argv);
		return(1);
	}

	model = get_model();
	if(!strstr(model, "7970")) {
		fprintf(stderr, "Not supported on model \"%s\"\n", model);
		return 1;
	}

	twifd = silabs_init();
	if(twifd == -1)
		return 1;

	while((c = getopt_long(argc, argv, "is:h", long_options, NULL)) != -1) {
		switch (c) {
		case 'i':
			do_info(twifd);
			break;
		case 's':
			do_sleep(twifd, atoi(optarg));
			break;
		case 'h':
		default:
			usage(argv);
		}
	}

	return 0;
}

#endif