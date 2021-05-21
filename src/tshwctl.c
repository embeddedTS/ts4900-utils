#include <asm-generic/termbits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <linux/types.h>
#include <math.h>

#include "gpiolib.h"
#include "fpga.h"
#include "crossbar-ts4900.h"
#include "crossbar-ts7970.h"
#include "crossbar-ts7990.h"

static int twifd;

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
		"  -i, --info             Print board revisions\n"
		"  -m, --addr <address>   Sets up the address for a peek/poke\n"
		"  -v, --poke <value>     Writes the value to the specified address\n"
		"  -t, --peek             Reads from the specified address\n"
		"  -l, --mode <8n1>       Used with -a, sets mode like '8n1', '7e2', etc\n"
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
	int opt_addr = 0, opt_info = 0;
	int opt_poke = 0, opt_peek = 0, opt_auto485 = 0;
	int baud = 0;
	int model;
	uint8_t pokeval = 0;
	char *uartmode = 0;
	struct cbarpin *cbar_inputs, *cbar_outputs;
	int cbar_size, cbar_mask;

	static struct option long_options[] = {
		{ "addr", 1, 0, 'm' },
		{ "poke", 1, 0, 'v' },
		{ "peek", 0, 0, 't' },
		{ "info", 0, 0, 'i' },
		{ "baud", 1, 0, 'x' },
		{ "mode", 1, 0, 'l' },
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
	if(model == 0x4900) {
		cbar_inputs = ts4900_inputs; 
		cbar_outputs = ts4900_outputs;
		cbar_size = 5;
		cbar_mask = 7;
	} else if(model == 0x7970) {
		cbar_inputs = ts7970_inputs; 
		cbar_outputs = ts7970_outputs;
		cbar_size = 6;
		cbar_mask = 3;
	} else if(model == 0x7990) {
		cbar_inputs = ts7990_inputs; 
		cbar_outputs = ts7990_outputs;
		cbar_size = 6;
		cbar_mask = 3;
	} else {
		fprintf(stderr, "Unsupported model %d\n", model);
		return 1;
	}

	if(twifd == -1) {
		perror("Can't open FPGA I2C bus");
		return 1;
	}

	while((c = getopt_long(argc, argv, "+m:v:il:x:ta:cgsqh", long_options, NULL)) != -1) {
		int gpiofd;
		int gpio, i;
		int uart;

		switch(c) {

		case 'm':
			opt_addr = 1;
			addr = strtoull(optarg, NULL, 0);
			break;
		case 'v':
			opt_poke = 1;
			pokeval = strtoull(optarg, NULL, 0);
			break;
		case 'l':
			uartmode = strdup(optarg);
			break;
		case 'x':
			baud = atoi(optarg);
			break;
		case 't':
			opt_peek = 1;
			break;
		case 'i':
			opt_info = 1;
			break;
		case 'a':
			opt_auto485 = atoi(optarg);
			break;
		case 'g':
			for (i = 0; cbar_inputs[i].name != 0; i++)
			{
				int j;
				uint8_t mode = fpeek8(twifd, cbar_inputs[i].addr) >> (8 - cbar_size);
				for (j = 0; cbar_outputs[j].name != 0; j++)
				{
					if(cbar_outputs[j].addr == mode){
						printf("%s=%s\n", cbar_inputs[i].name, cbar_outputs[j].name);
						break;
					}
				}
			}
			break;
		case 's':
			for (i = 0; cbar_inputs[i].name != 0; i++)
			{
				char *value = getenv(cbar_inputs[i].name);
				int j;
				if(value != NULL) {
					for (j = 0; cbar_outputs[j].name != 0; j++) {
						if(strcmp(cbar_outputs[j].name, value) == 0) {
							int mode = cbar_outputs[j].addr;
							uint8_t val = fpeek8(twifd, cbar_inputs[i].addr);
							fpoke8(twifd, cbar_inputs[i].addr, 
								   (mode << (8 - cbar_size)) | (val & cbar_mask));

							break;
						}
					}
					if(cbar_outputs[i].name == 0) {
						fprintf(stderr, "Invalid value \"%s\" for input %s\n",
							value, cbar_inputs[i].name);
					}
				}
			}
			break;
		case 'c':
			i = 0;
			printf("%13s (DIR) (VAL) FPGA Output\n", "FPGA Pad");
			for (i = 0; cbar_inputs[i].name != 0; i++)
			{
				uint8_t value = fpeek8(twifd, cbar_inputs[i].addr);
				uint8_t mode = value >> (8 - cbar_size);
				char *dir = value & 0x1 ? "out" : "in";
				int val;
				int j;

				// 4900 uses 5 bits for cbar, 7970/7990 use 6 and share
				// the data bit for input/output
				if(value & 0x1 || cbar_size == 6) {
					val = value & 0x2 ? 1 : 0;
				} else {
					val = value & 0x4 ? 1 : 0;
				}

				for (j = 0; cbar_outputs[j].name != 0; j++)
				{
					if(cbar_outputs[j].addr == mode){
						printf("%13s (%3s) (%3d) %s\n", 
							cbar_inputs[i].name,
							dir,
							val,
							cbar_outputs[j].name);
						break;
					}
				}

			}
			break;
		case 'q':
			printf("FPGA Inputs:\n");
			for (i = 0; cbar_inputs[i].name != 0; i++) {
				printf("%s\n", cbar_inputs[i].name);
			}
			printf("\nFPGA Outputs:\n");
			for (i = 0; cbar_outputs[i].name != 0; i++) {
				printf("%s\n", cbar_outputs[i].name);
			}
			break;
		default:
			usage(argv);
		}
	}

	if(opt_info) {
		uint8_t fpgarev = 0;
		char pcbrev = 0;
		uint8_t boardopt = 0;

		if(model == 0x4900) {
			uint8_t val;

			val = fpeek8(twifd, 51);
			fpgarev = (val >> 4) & 0xf;
			printf("n14=%d\n", !(val & 0x1));
			printf("l14=%d\n", !(val & 0x2));
			printf("g1=%d\n", !(val & 0x4));
			printf("b1=%d\n", !(val & 0x8));

			gpio_export(43);
			gpio_export(165);
			gpio_export(29);
			gpio_direction(43, 0);
			gpio_direction(165, 0);
			gpio_direction(29, 0);

			if(!gpio_read(29)) {
				pcbrev = 'E';
			} else if(gpio_read(43)) {
				pcbrev = 'A';
			} else {
				if(gpio_read(165)) {
					pcbrev = 'C';
				} else {
					pcbrev = 'D';
				}
			}
		} else if(model == 0x7970) {
			uint8_t r39, r37, r36, r34;
			uint8_t val;

			val = fpeek8(twifd, 51);
			fpgarev = (val >> 4) & 0xf;
			r37 = !(val & 0x1);
			r36 = !(val & 0x2);
			r34 = !(val & 0x4);
			r39 = !(val & 0x8);

			boardopt = r34 | (r36 << 1) | (r37 << 2) | (r39 << 3);

			gpio_export(193);
			gpio_export(192);
			gpio_direction(193, 0);
			gpio_direction(192, 0);

			if(gpio_read(193)) {
				pcbrev = 'A';
			} else {
				if(gpio_read(192)) {
					pcbrev = 'B';
				} else {
					pcbrev = 'D';
				}
			}			
		} else if(model == 0x7990) {
			uint8_t h12, g12, p13, l14;
			uint8_t val;

			val = fpeek8(twifd, 51);
			fpgarev = (val >> 4) & 0xf;
			h12 = !!(val & 0x1);
			g12 = !!(val & 0x2);
			l14 = !!(val & 0x4);
			p13 = !!(val & 0x8);
			boardopt = l14 | (p13 << 1) | (h12 << 2);

			val = fpeek8(twifd, 57);
			printf("okaya_present=%d\n", !!(val & 0x8));
			printf("lxd_present=%d\n", !!(val & 0x10));

			gpio_export(66);
			gpio_direction(66, 0);
			if(gpio_read(66) == 1) {
				pcbrev = 'A';
			} else {
				/* Rev < 10 couldn't read build resistors due
				 * to missing pullups on the fpga, but this
				 * fpga should only ship with rev b */
				if(fpgarev < 10) {
					pcbrev = 'B';
				} else {
					if(!g12) {
						pcbrev = 'B';
					} else {
						pcbrev = 'C';
					}
				}
			}
		}
		printf("model=%X\n", model);
		printf("fpgarev=%d\n", fpgarev);
		printf("pcbrev=%c\n", pcbrev);
		printf("boardopt=%d\n", boardopt);
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
		if(opt_auto485 != 1 && opt_auto485 != 3) {
			fprintf(stderr, "Specify value 1 or 3 for ttymxc1 or ttymxc3 (specified %d)\n", opt_auto485);
			return 1;
		}
		auto485_en(opt_auto485, baud, uartmode);
	}

	close(twifd);

	return 0;
}

