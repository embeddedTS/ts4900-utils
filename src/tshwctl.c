#include <asm/termbits.h>
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
#include <gpiod.h>
#include <assert.h>
#include <unistd.h>

#include "fpga.h"
#include "crossbar-ts4900.h"
#include "crossbar-ts7970.h"
#include "crossbar-ts7990.h"

static int i2cfd;

int get_model()
{
	int fd;
	char mdl[256] = { 0 }; // Ensure the buffer is zero-initialized
	char *ptr;
	ssize_t ret;

	// Open the model file using open system call
	fd = open("/proc/device-tree/model", O_RDONLY);
	if (fd < 0) {
		perror("model");
		return 0;
	}

	// Read the contents of the model file
	ret = read(fd, mdl, sizeof(mdl) - 1); // Read up to 255 bytes
	close(fd); // Close the file after reading

	if (ret < 0) {
		perror("read");
		return 0;
	}

	// Null-terminate the string
	mdl[ret] = '\0';

	// Find the "TS-" substring
	ptr = strstr(mdl, "TS-");
	if (!ptr) {
		fprintf(stderr, "Unsupported model: %s\n", mdl);
		return 0;
	}

	// Convert the number after "TS-" to a hexadecimal value
	return (int)strtoul(ptr + 3, NULL, 16);
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
	clk = (bits * 100000000) / baud;
	*cnt1 = lround((clk * 9.5) / fpga_clk);
	*cnt2 = lround((clk * 0.5) / fpga_clk);
}

void auto485_en(int uart, int baud, char *mode)
{
	struct termios2 tio;
	int uartfd;
	int symsz;
	uint32_t cnt1, cnt2;

	if (uart == 1)
		uartfd = open("/dev/ttymxc1", O_RDONLY);
	else if (uart == 3)
		uartfd = open("/dev/ttymxc3", O_RDONLY);
	else {
		fprintf(stderr, "Invalid UART specified\n");
		exit(1);
	}

	if (!uartfd) {
		perror("uart open");
		exit(1);
	}

	ioctl(uartfd, TCGETS2, &tio);
	if (baud != 0 && mode != NULL) {
		// If they did specify them, parse the symbol size
		// from the mode and set the new termios
		tio.c_ospeed = baud;
		tio.c_ispeed = baud;

		tio.c_cflag &= ~CSIZE;
		if (mode[0] == '8')
			tio.c_cflag |= CS8;
		else if (mode[0] == '7')
			tio.c_cflag |= CS7;
		else if (mode[0] == '6')
			tio.c_cflag |= CS6;
		else if (mode[0] == '5')
			tio.c_cflag |= CS5;
		if (mode[1] == 'n')
			tio.c_cflag &= ~PARENB;
		else if (mode[1] == 'e') {
			tio.c_cflag |= PARENB;
			tio.c_cflag &= ~PARODD;
		} else if (mode[1] == 'o')
			tio.c_cflag |= PARENB | PARODD;
		if (mode[2] == '1')
			tio.c_cflag &= ~CSTOPB;
		else if (mode[2] == '2')
			tio.c_cflag |= CSTOPB;
		if (ioctl(uartfd, TCSETS2, &tio) < 0) {
			perror("ioctl");
			exit(1);
		}
	}

	// If they didnt specify a mode & baud,
	// just read the current ioctl state and
	// go with that
	baud = tio.c_ospeed;
	if ((tio.c_cflag & CS8) == CS8)
		symsz = 10;
	else if ((tio.c_cflag & CS7) == CS7)
		symsz = 9;
	else if ((tio.c_cflag & CS6) == CS6)
		symsz = 8;
	else if ((tio.c_cflag & CS5) == CS5)
		symsz = 7;
	if (tio.c_cflag & CSTOPB)
		symsz++;
	if (tio.c_cflag & PARENB)
		symsz++;

	close(uartfd);

	if (mode == NULL) {
		printf("Setting Auto TXEN for %d baud and %d bits per symbol from current settings\n", baud, symsz);
	} else {
		printf("Setting Auto TXEN for %d baud and %d bits per symbol (%s)\n", baud, symsz, mode);
	}

	if (uart == 1) {
		autotx_bitstoclks(symsz, baud, &cnt1, &cnt2);
		fpoke8(i2cfd, 32, (uint8_t)((cnt1 & 0xff0000) >> 16));
		fpoke8(i2cfd, 33, (uint8_t)((cnt1 & 0xff00) >> 8));
		fpoke8(i2cfd, 34, (uint8_t)(cnt1 & 0xff));
		fpoke8(i2cfd, 35, (uint8_t)((cnt2 & 0xff0000) >> 16));
		fpoke8(i2cfd, 36, (uint8_t)((cnt2 & 0xff00) >> 8));
		fpoke8(i2cfd, 37, (uint8_t)(cnt2 & 0xff));
	} else if (uart == 3) {
		autotx_bitstoclks(symsz, baud, &cnt1, &cnt2);
		fpoke8(i2cfd, 38, (uint8_t)((cnt1 & 0xff0000) >> 16));
		fpoke8(i2cfd, 39, (uint8_t)((cnt1 & 0xff00) >> 8));
		fpoke8(i2cfd, 40, (uint8_t)(cnt1 & 0xff));
		fpoke8(i2cfd, 41, (uint8_t)((cnt2 & 0xff0000) >> 16));
		fpoke8(i2cfd, 42, (uint8_t)((cnt2 & 0xff00) >> 8));
		fpoke8(i2cfd, 43, (uint8_t)(cnt2 & 0xff));
	}
}

int do_ts7990_info(int i2cfd)
{
	struct gpiod_chip *cpu_chip1 = 0, *cpu_chip2 = 0, *cpu_chip4 = 0;
	struct gpiod_line *rev_b_line = 0, *rev_d_line = 0, *rev_e_line = 0;
	uint8_t h12, g12, p13, l14;
	uint8_t boardopt;
	uint8_t fpgarev;
	char pcbrev;
	uint8_t val;
	int value;
	int ret = 0;

	val = fpeek8(i2cfd, 51);
	fpgarev = (val >> 4) & 0xf;
	h12 = !!(val & 0x1);
	g12 = !!(val & 0x2);
	l14 = !!(val & 0x4);
	p13 = !!(val & 0x8);
	boardopt = l14 | (p13 << 1) | (h12 << 2);

	val = fpeek8(i2cfd, 57);
	printf("okaya_present=%d\n", !!(val & 0x8));
	printf("lxd_present=%d\n", !!(val & 0x10));

	cpu_chip1 = gpiod_chip_open("/dev/gpiochip1");
	if (!cpu_chip1) {
		perror("chip1: gpiod_chip_open");
		ret = 1;
		goto cleanup;
	}

	cpu_chip2 = gpiod_chip_open("/dev/gpiochip2");
	if (!cpu_chip2) {
		perror("chip2: gpiod_chip_open");
		ret = 1;
		goto cleanup;
	}

	cpu_chip4 = gpiod_chip_open("/dev/gpiochip4");
	if (!cpu_chip4) {
		perror("chip4: gpiod_chip_open");
		ret = 1;
		goto cleanup;
	}

	rev_b_line = gpiod_chip_get_line(cpu_chip2, 2);
	rev_d_line = gpiod_chip_get_line(cpu_chip4, 30);
	rev_e_line = gpiod_chip_get_line(cpu_chip1, 3);
	if (!rev_b_line || !rev_d_line || !rev_e_line) {
		perror("gpiod_chip_get_line");
		ret = 1;
		goto cleanup;
	}

	if (gpiod_line_request_input(rev_b_line, "tshwctl") < 0 ||
	    gpiod_line_request_input(rev_d_line, "tshwctl") < 0 ||
	    gpiod_line_request_input(rev_e_line, "tshwctl") < 0) {
		perror("gpiod_line_request_input");
		ret = 1;
		goto cleanup;
	}

	value = gpiod_line_get_value(rev_b_line);
	if (value < 0) {
		perror("gpiod_line_get_value");
		ret = 1;
		goto cleanup;
	}

	if (value) {
		pcbrev = 'A';
	} else {
		/* Rev < 10 couldn't read build resistors due
		 * to missing pullups on the fpga, but this
		 * fpga should only ship with rev b */

		if (fpgarev < 10 || !g12) {
			pcbrev = 'B';
		} else {
			value = gpiod_line_get_value(rev_d_line);
			if (value < 0) {
				perror("gpiod_line_get_value");
				ret = 1;
				goto cleanup;
			}

			if (value == 0) {
				value = gpiod_line_get_value(rev_e_line);
				if (value < 0) {
					perror("gpiod_line_get_value");
					ret = 1;
					goto cleanup;
				}

				if (value == 0) {
					pcbrev = 'E';
				} else {
					pcbrev = 'D';
				}

			} else {
				pcbrev = 'C';
			}
		}
	}

	printf("model=0x7990\n");
	printf("fpgarev=%d\n", fpgarev);
	printf("pcbrev=%c\n", pcbrev);
	printf("boardopt=%d\n", boardopt);

cleanup:
	if (cpu_chip1)
		gpiod_chip_close(cpu_chip1);
	if (cpu_chip2)
		gpiod_chip_close(cpu_chip2);
	if (cpu_chip4)
		gpiod_chip_close(cpu_chip4);

	return ret;
}

int do_ts7970_info(int i2cfd)
{
	struct gpiod_chip *cpu_chip0 = 0, *cpu_chip1 = 0, *cpu_chip6 = 0;
	struct gpiod_line *rev_b_line = 0, *rev_d_line = 0, *rev_g_line = 0, *rev_h_line = 0;
	uint8_t r39, r37, r36, r34;
	uint8_t boardopt;
	uint8_t fpgarev;
	char pcbrev;
	uint8_t val;
	int value;
	int ret = 0;

	cpu_chip0 = gpiod_chip_open("/dev/gpiochip0");
	if (!cpu_chip0) {
		perror("chip0: gpiod_chip_open");
		ret = 1;
		goto cleanup;
	}

	cpu_chip1 = gpiod_chip_open("/dev/gpiochip1");
	if (!cpu_chip1) {
		perror("chip1: gpiod_chip_open");
		ret = 1;
		goto cleanup;
	}

	cpu_chip6 = gpiod_chip_open("/dev/gpiochip6");
	if (!cpu_chip6) {
		perror("chip6: gpiod_chip_open");
		ret = 1;
		goto cleanup;
	}

	val = fpeek8(i2cfd, 51);
	fpgarev = (val >> 4) & 0xf;
	r37 = !(val & 0x1);
	r36 = !(val & 0x2);
	r34 = !(val & 0x4);
	r39 = !(val & 0x8);

	boardopt = r34 | (r36 << 1) | (r37 << 2) | (r39 << 3);

	rev_b_line = gpiod_chip_get_line(cpu_chip6, 1);
	rev_d_line = gpiod_chip_get_line(cpu_chip6, 0);
	rev_g_line = gpiod_chip_get_line(cpu_chip0, 29);
	rev_h_line = gpiod_chip_get_line(cpu_chip1, 3);
	if (!rev_b_line || !rev_d_line || !rev_g_line || !rev_h_line) {
		perror("gpiod_chip_get_line");
		ret = 1;
		goto cleanup;
	}

	if (gpiod_line_request_input(rev_b_line, "tshwctl") < 0 ||
	    gpiod_line_request_input(rev_d_line, "tshwctl") < 0 ||
	    gpiod_line_request_input(rev_g_line, "tshwctl") < 0 ||
	    gpiod_line_request_input(rev_h_line, "tshwctl") < 0) {
		perror("gpiod_line_request_input");
		ret = 1;
		goto cleanup;
	}

	value = gpiod_line_get_value(rev_h_line);
	if (value < 0) {
		perror("gpiod_line_get_value");
		ret = 1;
		goto cleanup;
	}

	if (value == 0) {
		pcbrev = 'H';
	} else {
		value = gpiod_line_get_value(rev_g_line);
		if (value < 0) {
			perror("gpiod_line_get_value");
			ret = 1;
			goto cleanup;
		}

		if (value == 0) {
			pcbrev = 'G';
		} else {
			/* REV F required a fuse to check */
			int fusefd;
			char buf[64];
			uint32_t val;

			fusefd = open("/sys/fsl_otp/HW_OCOTP_GP1", O_RDONLY);
			if (fusefd != -1) {
				assert(fusefd != 0);
				int i = read(fusefd, &buf, 64);
				if (i < 1) {
					perror("Couldn't read fuses");
					exit(1);
				}
				val = strtoull(buf, NULL, 0);
			} else {
				fusefd = open("/sys/bus/nvmem/devices/imx-ocotp0/nvmem", O_RDONLY);
				if (fusefd != -1) {
					/* (0x660-0x400) >> 4 is word 0x23 (GP1) */
					off_t offset = lseek(fusefd, 0x23 * 4, SEEK_SET);
					assert(offset != -1);
					int i = read(fusefd, &val, sizeof(val));
					if (i < 1) {
						perror("Couldn't read fuses");
						exit(1);
					}
				} else {
					fprintf(stderr, "Fuse driver not enabled, can't detect PCB revision\n");
					return 1;
				}
			}
			close(fusefd);

			if (val & 0x1) {
				pcbrev = 'F';
			} else {
				value = gpiod_line_get_value(rev_b_line);
				if (value < 0) {
					perror("gpiod_line_get_value");
					ret = 1;
					goto cleanup;
				}

				if (value) {
					pcbrev = 'A';
				} else {
					value = gpiod_line_get_value(rev_d_line);
					if (value < 0) {
						perror("gpiod_line_get_value");
						ret = 1;
						goto cleanup;
					}
					if (value) {
						pcbrev = 'B';
					} else {
						pcbrev = 'D';
					}
				}
			}
		}
	}

	printf("model=0x7970\n");
	printf("fpgarev=%d\n", fpgarev);
	printf("pcbrev=%c\n", pcbrev);
	printf("boardopt=%d\n", boardopt);

cleanup:
	if (cpu_chip0)
		gpiod_chip_close(cpu_chip0);
	if (cpu_chip1)
		gpiod_chip_close(cpu_chip1);
	if (cpu_chip6)
		gpiod_chip_close(cpu_chip6);

	return ret;
}

int do_ts4900_info(int i2cfd)
{
	struct gpiod_chip *cpu_chip0 = 0, *cpu_chip1 = 0, *cpu_chip5 = 0;
	struct gpiod_line *rev_e_line = 0, *rev_b_line = 0, *rev_d_line = 0;
	uint8_t fpgarev;
	char pcbrev;
	uint8_t val;
	int value;
	int ret = 0;

	val = fpeek8(i2cfd, 51);
	fpgarev = (val >> 4) & 0xf;
	printf("n14=%d\n", !(val & 0x1));
	printf("l14=%d\n", !(val & 0x2));
	printf("g1=%d\n", !(val & 0x4));
	printf("b1=%d\n", !(val & 0x8));

	cpu_chip0 = gpiod_chip_open("/dev/gpiochip0");
	if (!cpu_chip0) {
		perror("chip0: gpiod_chip_open");
		ret = 1;
		goto cleanup;
	}

	cpu_chip1 = gpiod_chip_open("/dev/gpiochip1");
	if (!cpu_chip1) {
		perror("chip1: gpiod_chip_open");
		ret = 1;
		goto cleanup;
	}

	cpu_chip5 = gpiod_chip_open("/dev/gpiochip5");
	if (!cpu_chip5) {
		perror("chip5: gpiod_chip_open");
		ret = 1;
		goto cleanup;
	}

	rev_b_line = gpiod_chip_get_line(cpu_chip1, 11);
	rev_d_line = gpiod_chip_get_line(cpu_chip5, 5);
	rev_e_line = gpiod_chip_get_line(cpu_chip0, 29);
	if (!rev_d_line || !rev_b_line || !rev_e_line) {
		perror("gpiod_chip_get_line");
		ret = 1;
		goto cleanup;
	}

	if (gpiod_line_request_input(rev_e_line, "tshwctl") < 0 ||
	    gpiod_line_request_input(rev_d_line, "tshwctl") < 0 ||
	    gpiod_line_request_input(rev_b_line, "tshwctl") < 0) {
		perror("gpiod_line_request_input");
		ret = 1;
		goto cleanup;
	}

	value = gpiod_line_get_value(rev_e_line);
	if (value < 0) {
		perror("gpiod_line_get_value");
		ret = 1;
		goto cleanup;
	}
	if (!value) {
		pcbrev = 'E';
	} else {
		value = gpiod_line_get_value(rev_b_line);
		if (value < 0) {
			perror("gpiod_line_get_value");
			ret = 1;
			goto cleanup;
		}
		if (value) {
			pcbrev = 'A';
		} else {
			value = gpiod_line_get_value(rev_d_line);
			if (value < 0) {
				perror("gpiod_line_get_value");
				ret = 1;
				goto cleanup;
			}

			if (value) {
				pcbrev = 'C';
			} else {
				pcbrev = 'D';
			}
		}
	}

	printf("model=0x4900\n");
	printf("fpgarev=%d\n", fpgarev);
	printf("pcbrev=%c\n", pcbrev);

cleanup:
	if (cpu_chip0)
		gpiod_chip_close(cpu_chip0);
	if (cpu_chip1)
		gpiod_chip_close(cpu_chip1);
	if (cpu_chip5)
		gpiod_chip_close(cpu_chip5);

	return ret;
}

void usage(char **argv)
{
	fprintf(stderr,
		"Usage: %s [OPTIONS] ...\n"
		"embeddedTS I2C FPGA Utility\n"
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
		argv[0]);
}

int main(int argc, char **argv)
{
	int c;
	uint16_t addr = 0x0;
	int opt_addr = 0, opt_info = 0;
	int opt_poke = 0, opt_peek = 0, opt_auto485 = 0;
	int baud = 0;
	int model;
	uint8_t pokeval = 0;
	char *uartmode = 0;
	struct cbarpin *cbar_inputs, *cbar_outputs;
	int cbar_size, cbar_mask;

	static struct option long_options[] = {
		{ "addr", 1, 0, 'm' }, { "poke", 1, 0, 'v' }, { "peek", 0, 0, 't' },	 { "info", 0, 0, 'i' },
		{ "baud", 1, 0, 'x' }, { "mode", 1, 0, 'l' }, { "autotxen", 1, 0, 'a' }, { "get", 0, 0, 'g' },
		{ "set", 0, 0, 's' },  { "dump", 0, 0, 'c' }, { "showall", 0, 0, 'q' },	 { "help", 0, 0, 'h' },
		{ 0, 0, 0, 0 }
	};

	i2cfd = fpga_init();
	model = get_model();
	if (model == 0x4900) {
		cbar_inputs = ts4900_inputs;
		cbar_outputs = ts4900_outputs;
		cbar_size = 5;
		cbar_mask = 7;
	} else if (model == 0x7970) {
		cbar_inputs = ts7970_inputs;
		cbar_outputs = ts7970_outputs;
		cbar_size = 6;
		cbar_mask = 3;
	} else if (model == 0x7990) {
		cbar_inputs = ts7990_inputs;
		cbar_outputs = ts7990_outputs;
		cbar_size = 6;
		cbar_mask = 3;
	} else {
		fprintf(stderr, "Unsupported model %d\n", model);
		return 1;
	}

	if (i2cfd == -1) {
		perror("Can't open FPGA I2C bus");
		return 1;
	}

	while ((c = getopt_long(argc, argv, "+m:v:il:x:ta:cgsqh", long_options, NULL)) != -1) {
		int i;

		switch (c) {
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
			for (i = 0; cbar_inputs[i].name != 0; i++) {
				int j;
				uint8_t mode = fpeek8(i2cfd, cbar_inputs[i].addr) >> (8 - cbar_size);
				for (j = 0; cbar_outputs[j].name != 0; j++) {
					if (cbar_outputs[j].addr == mode) {
						printf("%s=%s\n", cbar_inputs[i].name, cbar_outputs[j].name);
						break;
					}
				}
			}
			break;
		case 's':
			for (i = 0; cbar_inputs[i].name != 0; i++) {
				char *value = getenv(cbar_inputs[i].name);
				int j;
				if (value != NULL) {
					for (j = 0; cbar_outputs[j].name != 0; j++) {
						if (strcmp(cbar_outputs[j].name, value) == 0) {
							int mode = cbar_outputs[j].addr;
							uint8_t val = fpeek8(i2cfd, cbar_inputs[i].addr);
							fpoke8(i2cfd, cbar_inputs[i].addr,
							       (mode << (8 - cbar_size)) | (val & cbar_mask));

							break;
						}
					}
					if (cbar_outputs[i].name == 0) {
						fprintf(stderr, "Invalid value \"%s\" for input %s\n", value,
							cbar_inputs[i].name);
					}
				}
			}
			break;
		case 'c':
			i = 0;
			printf("%13s (DIR) (VAL) FPGA Output\n", "FPGA Pad");
			for (i = 0; cbar_inputs[i].name != 0; i++) {
				uint8_t value = fpeek8(i2cfd, cbar_inputs[i].addr);
				uint8_t mode = value >> (8 - cbar_size);
				char *dir = value & 0x1 ? "out" : "in";
				int val;
				int j;

				// 4900 uses 5 bits for cbar, 7970/7990 use 6 and share
				// the data bit for input/output
				if (value & 0x1 || cbar_size == 6) {
					val = value & 0x2 ? 1 : 0;
				} else {
					val = value & 0x4 ? 1 : 0;
				}

				for (j = 0; cbar_outputs[j].name != 0; j++) {
					if (cbar_outputs[j].addr == mode) {
						printf("%13s (%3s) (%3d) %s\n", cbar_inputs[i].name, dir, val,
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

	if (opt_info) {
		if (model == 0x4900) {
			return do_ts4900_info(i2cfd);
		} else if (model == 0x7970) {
			return do_ts7970_info(i2cfd);
		} else if (model == 0x7990) {
			return do_ts7990_info(i2cfd);
		}
	}

	if (opt_poke) {
		if (opt_addr) {
			fpoke8(i2cfd, addr, pokeval);
		} else {
			fprintf(stderr, "No address specified\n");
		}
	}

	if (opt_peek) {
		if (opt_addr) {
			printf("addr%d=0x%X\n", addr, fpeek8(i2cfd, addr));
		} else {
			fprintf(stderr, "No address specified\n");
		}
	}

	if (opt_auto485) {
		if (opt_auto485 != 1 && opt_auto485 != 3) {
			fprintf(stderr, "Specify value 1 or 3 for ttymxc1 or ttymxc3 (specified %d)\n", opt_auto485);
			return 1;
		}
		auto485_en(opt_auto485, baud, uartmode);
	}

	close(i2cfd);

	return 0;
}
