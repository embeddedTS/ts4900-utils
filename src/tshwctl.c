#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <asm-generic/termbits.h>
#include <asm-generic/ioctls.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <linux/types.h>
#include <math.h>

#include "fpga.h"
#include "crossbar-ts4900.h"

static twifd;

int fpgadio_read(int dio) {
	uint8_t data = fpeek8(twifd, dio);
	if(data & 0x4) return 1;
	else return 0;
}

int fpgadio_set(int dio, int value) {
	if(value) fpoke8(twifd, dio, 0x3);
	else fpoke8(twifd, dio, 0x1);
}

// Value 1 is output, 0 is input
int fpgadio_ddr(int dio, int value) {
	if(value) fpoke8(twifd, dio, 0x1);
	else fpoke8(twifd, dio, 0x0);
}

int get_model()
{
	FILE *proc;
	char mdl[256];
	char *ptr;
	int ret;

	proc = fopen("/proc/device-tree/model", "r");
	if (!proc) {
		perror("model");
		return 0;
	}
	fread(mdl, 256, 1, proc);
	ptr = strstr(mdl, "TS-");
	return strtoull(ptr+3, NULL, 16);
}

// Calculate the number of 24mhz clocks for a given
// baud rate / bits per symbol
// For example:
////// CNT 1
//// 115200 is 8681ns byte time
//// 8681*9.5=82469.5ns
//// 24mhz is 41.67ns
//// 82469.5/41.67 = 1979 (CNT1)
////// CNT 2
//// 4340.5ns is .5 bit time
//// 4340.5/41.67 = 104 (CNT2)
void autotx_bitstoclks(int bits, int baud, uint32_t *cnt1, uint32_t *cnt2)
{
	float clk;
	const float fpga_clk = 41.67; // ns

	// get baud byte time in ns
	clk = (bits*100000000)/baud;
	*cnt1 = lround((clk*9.5) / fpga_clk);
	*cnt2 = lround((clk*0.5) / fpga_clk);
}

void auto485_en(int uart, int baud, char *mode)
{
	struct termios2 tio;
	int uartfd;
	int symsz;
	uint32_t cnt1, cnt2;

	if(uart == 1)
		uartfd = open("/dev/ttymxc1", O_RDONLY);
	else if (uart == 3)
		uartfd = open("/dev/ttymxc3", O_RDONLY);

	ioctl(uartfd, TCGETS2, &tio);
	if(baud != 0 && mode != NULL) {
		// If they did specify them, parse the symbol size
		// from the mode and set the new termios
		tio.c_ospeed = baud;
		tio.c_ispeed = baud;

		tio.c_cflag &= ~CSIZE;
		if(mode[0] == '8') tio.c_cflag |= CS8;
		else if(mode[0] == '7') tio.c_cflag |= CS7;
		else if(mode[0] == '6') tio.c_cflag |= CS6;
		else if(mode[0] == '5') tio.c_cflag |= CS5;
		if(mode[1] == 'n') tio.c_cflag &= ~PARENB;
		else if(mode[1] == 'e'){
			tio.c_cflag |= PARENB;
			tio.c_cflag &= ~PARODD;
		} else if (mode[1] == 'o') tio.c_cflag |= PARENB | PARODD;
		if(mode[2] == '1') tio.c_cflag &= ~CSTOPB;
		else if (mode[2] == '2') tio.c_cflag |= CSTOPB;
		tcsetattr(uartfd, TCSETS2, &tio);
	}

	// If they didnt specify a mode & baud, 
	// just read the current ioctl state and 
	// go with that
	baud = tio.c_ospeed;
	if((tio.c_cflag & CS8) == CS8) symsz = 10;
	else if((tio.c_cflag & CS7) == CS7) symsz = 9;
	else if((tio.c_cflag & CS6) == CS6) symsz = 8;
	else if((tio.c_cflag & CS5) == CS5) symsz = 7;
	if(tio.c_cflag & CSTOPB) symsz++;
	if(tio.c_cflag & PARENB) symsz++;

	close(uartfd);

	if(mode == NULL) {
		printf("Setting Auto TXEN for %d baud and %d bits per symbol from current settings\n",
		baud, symsz);
	} else {
		printf("Setting Auto TXEN for %d baud and %d bits per symbol (%s)\n",
		baud, symsz, mode);
	}
	

	if(uart == 1) {
		autotx_bitstoclks(symsz, baud, &cnt1, &cnt2);
		fpoke8(twifd, 32, (uint8_t)((cnt1 & 0xff0000) >> 16));
		fpoke8(twifd, 33, (uint8_t)((cnt1 & 0xff00) >> 8));
		fpoke8(twifd, 34, (uint8_t)(cnt1 & 0xff));
		fpoke8(twifd, 35, (uint8_t)((cnt2 & 0xff0000) >> 16));
		fpoke8(twifd, 36, (uint8_t)((cnt2 & 0xff00) >> 8));
		fpoke8(twifd, 37, (uint8_t)(cnt2 & 0xff));
	} else if (uart == 3) {
		autotx_bitstoclks(symsz, baud, &cnt1, &cnt2);
		fpoke8(twifd, 38, (uint8_t)((cnt1 & 0xff0000) >> 16));
		fpoke8(twifd, 39, (uint8_t)((cnt1 & 0xff00) >> 8));
		fpoke8(twifd, 40, (uint8_t)(cnt1 & 0xff));
		fpoke8(twifd, 41, (uint8_t)((cnt2 & 0xff0000) >> 16));
		fpoke8(twifd, 42, (uint8_t)((cnt2 & 0xff00) >> 8));
		fpoke8(twifd, 43, (uint8_t)(cnt2 & 0xff));
	}
}


void usage(char **argv) {
	fprintf(stderr,
		"Usage: %s [OPTIONS] ...\n"
		"Technologic Systems I2C FPGA Utility\n"
		"\n"
		/*  Use the sysfs interface, not these
		"  -p, --getin <dio>      Returns the input value of an FPGA DIO\n"
		"  -e, --setout <dio>     Sets an FPGA DIO output value high\n"
		"  -l, --clrout <dio>     Sets an FPGA DIO output value low\n"
		"  -d, --ddrout <dio>     Set FPGA DIO to an output\n"
		"  -r, --ddrin <dio>      Set FPGA DIO to an input\n"*/
		"  -m, --addr <address>   Sets up the address for a peek/poke\n"
		"  -v, --poke <value>     Writes the value to the specified address\n"
		"  -t, --peek             Reads from the specified address\n"
		"  -i, --mode <8n1>       Used with -a, sets mode like '8n1', '7e2', etc\n"
		"  -x, --baud <speed>     Used with -a, sets baud rate for auto485\n"
		"  -a, --autotxen <uart>  Enables autotxen for supported CPU UARTs\n"
		"                           Uses baud/mode if set or reads the current\n"
		"                           configuration of that uart\n"
		"  -c, --dump             Prints out the crossbar configuration\n"
		"  -g, --get              Print crossbar for use in eval\n"
		"  -s, --set              Read environment for crossbar changes\n"
        "  -q, --showall          Print all possible FPGA inputs and outputs.\n"
		"  -h, --help             This message\n"
		"\n",
		argv[0]
	);
}

int main(int argc, char **argv) 
{
	int c;
	uint16_t addr = 0x0, val;
	int opt_addr = 0;
	int opt_poke = 0, opt_peek = 0, opt_auto485 = 0;
	int baud = 0;
	int model;
	uint8_t pokeval = 0;
	char *uartmode = 0;

	static struct option long_options[] = {
		{ "getin", 1, 0, 'p' },
		{ "setout", 1, 0, 'e' },
		{ "clrout", 1, 0, 'l' },
		{ "ddrout", 1, 0, 'd' },
		{ "ddrin", 1, 0, 'r' },
		{ "addr", 1, 0, 'm' },
		{ "poke", 1, 0, 'v' },
		{ "peek", 0, 0, 't' },
		{ "baud", 1, 0, 'x' },
		{ "mode", 1, 0, 'i' },
		{ "autotxen", 1, 0, 'a' },
		{ "get", 0, 0, 'g' },
		{ "set", 0, 0, 's' },
		{ "dump", 0, 0, 'c' },
		{ "showall", 0, 0, 'q' },
		{ "help", 0, 0, 'h' },
		{ 0, 0, 0, 0 }
	};

	twifd = fpga_init();
	model = get_model();

	if(twifd == -1) {
		perror("Can't open FPGA I2C bus");
		return 1;
	}

	while((c = getopt_long(argc, argv, "+p:e:l:d:r:m:v:i:x:ta:cgsqh", long_options, NULL)) != -1) {
		int gpiofd;
		int gpio, i;
		int uart;

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
		case 'm':
			opt_addr = 1;
			addr = strtoull(optarg, NULL, 0);
			break;
		case 'v':
			opt_poke = 1;
			pokeval = strtoull(optarg, NULL, 0);
			break;
		case 'i':
			uartmode = strdup(optarg);
			break;
		case 'x':
			baud = atoi(optarg);
			break;
		case 't':
			opt_peek = 1;
			break;
		case 'a':
			opt_auto485 = atoi(optarg);
			break;
		case 'g':
			for (i = 0; ts4900_inputs[i].name != 0; i++)
			{
				uint8_t mode = fpeek8(twifd, ts4900_inputs[i].addr) >> 3;
				printf("%s=%s\n", ts4900_inputs[i].name, ts4900_outputs[mode].name);
			}
			break;
		case 's':
			for (i = 0; ts4900_inputs[i].name != 0; i++)
			{
				char *value = getenv(ts4900_inputs[i].name);
				int j;
				if(value != NULL) {
					for (j = 0; ts4900_outputs[j].name != 0; j++) {
						if(strcmp(ts4900_outputs[j].name, value) == 0) {
							int mode = ts4900_outputs[j].addr;
							uint8_t val = fpeek8(twifd, ts4900_inputs[i].addr);
							fpoke8(twifd, ts4900_inputs[i].addr, (mode << 3) | (val & 0x7));
							break;
						}
					}
					if(ts4900_outputs[i].name == 0) {
						fprintf(stderr, "Invalid value \"%s\" for input %s\n",
							value, ts4900_inputs[i].name);
					}
				}
			}
			break;
		case 'c':
			i = 0;
			printf("%11s (DIR) (VAL) FPGA Input\n", "FPGA Output");
			for (i = 0; ts4900_inputs[i].name != 0; i++)
			{
				uint8_t value = fpeek8(twifd, ts4900_inputs[i].addr);
				uint8_t mode = value >> 3;
				char *dir = value & 0x1 ? "out" : "in";
				int val;
				if(value & 0x1) {
					val = value & 0x2 ? 1 : 0;
				} else {
					val = value & 0x4 ? 1 : 0;
				}
				printf("%11s (%3s) (%3d) %s\n", 
					ts4900_inputs[i].name,
					dir,
					val,
					ts4900_outputs[mode].name);
			}
			break;
		case 'q':
			printf("FPGA Inputs:\n");
			for (i = 0; ts4900_inputs[i].name != 0; i++) {
				printf("%s\n", ts4900_inputs[i].name);
			}
			printf("\nFPGA Outputs:\n");
			for (i = 0; ts4900_outputs[i].name != 0; i++) {
				printf("%s\n", ts4900_outputs[i].name);
			}
			break;
		default:
			usage(argv);
		}
	}

	if(opt_poke) {
		if(opt_addr) {
			fpoke8(twifd, addr, pokeval);
		} else {
			fprintf(stderr, "No address specified\n");
		}
	}

	if(opt_peek) {
		if(opt_addr) {
			printf("addr%d=0x%X\n", addr, fpeek8(twifd, addr));
		} else {
			fprintf(stderr, "No address specified\n");
		}
	}

	if(opt_auto485) {
		int uart;
		if(opt_auto485 != 1 && opt_auto485 != 3) {
			fprintf(stderr, "Specify value 1 or 3 for ttymxc1 or ttymxc3 (specified %d)\n", uart);
			return 1;
		}
		auto485_en(opt_auto485, baud, uartmode);
	}

	close(twifd);

	return 0;
}

