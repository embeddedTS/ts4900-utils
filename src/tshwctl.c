#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <linux/types.h>

#include "fpga.h"

static twifd = -1;

int fpgadio_read(int dio) {
	uint8_t data = fpeek16(twifd, dio);
	if(data & 0x4) return 1;
	else return 0;
}

int fpgadio_set(int dio, int value) {
	if(value) fpoke16(twifd, dio, 0x3);
	else fpoke16(twifd, dio, 0x1);
}

// Value 1 is output, 0 is input
int fpgadio_ddr(int dio, int value) {
	if(value) fpoke16(twifd, dio, 0x1);
	else fpoke16(twifd, dio, 0x0);
}

void usage(char **argv) {
	fprintf(stderr,
		"Usage: %s [OPTIONS] ...\n"
		"Technologic Systems TS-4900 Utility\n"
		"\n"
		"  -p, --getin <dio>      Returns the input value of an FPGA DIO\n"
		"  -e, --setout <dio>     Sets an FPGA DIO output value high\n"
		"  -l, --clrout <dio>     Sets an FPGA DIO output value low\n"
		"  -d, --ddrout <dio>     Set FPGA DIO to an output\n"
		"  -r, --ddrin <dio>      Set FPGA DIO to an input\n"
		"  -f, --bten <1|0>       Switches ttymxc2 to bluetooth\n"
		"  -n, --bbclk12 <1|0>    Adds a 12MHz CLK on CN1_87\n"
		"  -o, --bbclk14 <1|0>    Adds a 14.3MHz CLK on CN1_87\n"
		"  -u, --uart2en <1|0>    Switches ttymxc1 to cn2-78/80\n"
		"  -a, --uart4en <1|0>    Muxes ttymxc3 to cn2_86/88\n"
		"  -s, --pushsw           Returns the value of the push switch\n"
		"  -h, --help             This message\n"
		"\n",
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
		{ "bten", 1, 0, 'b' },
		{ "bbclk12", 1, 0, 'n' },
		{ "bbclk14", 1, 0, 'o' },
		{ "uart2en", 1, 0, 'u' },
		{ "uart4en", 1, 0, 'a' },
		{ "pushsw", 0, 0, 's' },
		{ "help", 0, 0, 'h' },
		{ 0, 0, 0, 0 }
	};

	if (argc == 1) {
		usage(argv);
		return 1;
	}
	twifd = fpga_init();
	if(twifd == -1) return 1;

	while((c = getopt_long(argc, argv, "p:e:l:d:r:b:n:sho:u:a:", long_options, NULL)) != -1) {
		int gpiofd;
		int gpio, i;

		switch(c) {
		case 'p':
			gpio = atoi(optarg);
			printf("gpio%d=%d\n", gpio, fpgadio_read(gpio));
			break;
		case 'e':
			gpio = atoi(optarg);
			fpgadio_set(gpio, 1);
			break;
		case 'l':
			gpio = atoi(optarg);
			fpgadio_set(gpio, 0);
			break;
		case 'd':
			gpio = atoi(optarg);
			fpgadio_ddr(gpio, 1);
			break;
		case 'r':
			gpio = atoi(optarg);
			fpgadio_ddr(gpio, 0);
			break;
		case 'f':
			i = atoi(optarg);
			fpoke16(twifd, 46, i);
			break;
		case 'n':
			i = atoi(optarg);
			fpoke16(twifd, 47, i);
			break;
		case 'o':
			i = atoi(optarg);
			fpoke16(twifd, 48, i);
			break;
		case 'u':
			i = atoi(optarg);
			fpoke16(twifd, 49, i);
			break;
		case 'a':
			i = atoi(optarg);
			fpoke16(twifd, 50, i);
			break;
		case 's':
			i = fpeek16(twifd, 31);
			printf("pushsw=%d\n", i ? 0 : 1);
			break;
		case 'h':
			usage(argv);
			break;
		default:
			usage(argv);
		}
	}

	close(twifd);

	return 0;
}

