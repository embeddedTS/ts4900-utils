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

uint16_t* sread(int twifd, uint16_t *data)
{
	uint8_t tmp[30];
	bzero(tmp, 30);
	int i;

    read(twifd, tmp, 30);
    for (i = 0; i < 15; i++)
    	data[i] = (tmp[i*2] << 8) | tmp[(i*2)+1];

	return data;
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

#ifdef CTL

void do_info(int twifd)
{
	uint16_t data[20];
	memset(data, 0, sizeof(uint16_t));
	sread(twifd, data);

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
		printf("V3P3=%d\n", sscale(data[14]));
	}
}

void do_read(int twifd)
{
	printf("input,value\n");
	while(1) {
		uint16_t data[20];
		sread(twifd, data);
		printf("p10,0x%X\n", data[4]);
		printf("p11,0x%X\n", data[5]);
		printf("p12,0x%X\n", data[6]);
	}
}

static void usage(char **argv) {
	fprintf(stderr, "Usage: %s [OPTION] ...\n"
	  "Technologic Systems Microcontroller Access\n"
	  "\n"
	  "  -h, --help            This message\n"
	  "  -i, --info            Read all Silabs ADC values\n"
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

	while((c = getopt_long(argc, argv, "ih", long_options, NULL)) != -1) {
		switch (c) {
		case 'i':
			do_info(twifd);
			break;
		case 'h':
		default:
			usage(argv);
		}
	}

	return 0;
}

#endif