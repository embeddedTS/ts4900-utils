#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <string.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include "micro.h"

int model = 0;
int i2cdevaddr;

static int get_model()
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

// Scale voltage to microcontroller 0-2.5V
static uint16_t inline sscale(uint16_t data)
{
	return data * (2.5 / 1023) * 1000;
}

// Scale voltage for resistor dividers
static uint16_t inline rscale(uint16_t data, uint16_t r1, uint16_t r2)
{
	uint16_t ret = (data * (r1 + r2) / r2);
	return sscale(ret);
}

// Scale for 0-20mA current loop
// shunt in ohms
static uint16_t inline cscale(uint16_t data, uint16_t shunt)
{
	uint32_t ret = sscale(data);
	// Scale to microamps
	ret *= 1000;
	ret /= shunt;

	return (uint16_t)ret;
}

static int do_sleep(int i2cfd, int seconds)
{
	unsigned char dat[4] = { 0 };
	int opt_resetswitchwkup = 1;
	struct i2c_rdwr_ioctl_data ioctl_data;
	struct i2c_msg msgs[1];

	if (model == 0x7990) {
		/* On the 7990 enable irq on touch controller */
		dat[0] = 51;
		dat[1] = 0x1;
		msgs[0].addr = 0x5c;
		msgs[0].flags = 0;
		msgs[0].len = 2;
		msgs[0].buf = (uint8_t *)dat;
		ioctl_data.msgs = msgs;
		ioctl_data.nmsgs = 1;
		if (ioctl(i2cfd, I2C_RDWR, &ioctl_data) < 0) {
			perror("I2C_RDWR ioctl transaction failed");
			return -1;
		}

		dat[0] = 52;
		dat[1] = 0xa;
		msgs[0].len = 2;
		msgs[0].buf = (uint8_t *)dat;
		if (ioctl(i2cfd, I2C_RDWR, &ioctl_data) < 0) {
			perror("I2C_RDWR ioctl transaction failed");
			return -1;
		}
	}

	// Sleep command
	dat[0] = (0x1 | (opt_resetswitchwkup << 1) | (1 << 6));
	dat[3] = (seconds & 0xff);
	dat[2] = ((seconds >> 8) & 0xff);
	dat[1] = ((seconds >> 16) & 0xff);

	if (v0_stream_write(i2cfd, i2cdevaddr, (uint8_t *)dat, 4) < 0) {
		fprintf(stderr, "Unable to issue sleep command\n");
		return -1;
	}

	return 0;
}

static uint16_t swap_byte_order(uint16_t value)
{
	return (value >> 8) | (value << 8);
}

static int do_ts7990_info(int i2cfd)
{
	uint16_t data[16];
	int i;

	if (v0_stream_read(i2cfd, i2cdevaddr, (uint8_t *)data, 32) < 0) {
		fprintf(stderr, "I2C Read failed\n");
		return -1;
	}
	for (i = 0; i <= 10; i++)
		data[i] = swap_byte_order(data[i]);

	printf("VIN=%d\n", rscale(data[0], 2870, 147));
	printf("V5_A=%d\n", rscale(data[1], 147, 107));
	printf("AN_LCD_20V=%d\n", rscale(data[2], 121, 1));
	printf("DDR_1P5V=%d\n", sscale(data[3]));
	printf("V1P8=%d\n", sscale(data[4]));
	printf("SUPERCAP=%d\n", rscale(data[5], 100, 22));
	printf("SUPERCAP_PCT=%d\n", (rscale(data[5], 100, 22) * 100) / 12000);
	printf("BACK_LT_RAW=0x%X\n", rscale(data[6], 2870, 147));
	printf("V3P3=%d\n", rscale(data[7], 499, 499));
	printf("VDD_ARM_CAP=%d\n", sscale(data[8]));
	printf("VDD_SOC_CAP=%d\n", sscale(data[9]));
	printf("MICROREV=%d\n", data[15] >> 8);

	return 0;
}

static int do_ts7970_info(int i2cfd)
{
	uint16_t data[19];
	int i;

	/* Rev 6 marks the change from Silicon Labs parts to Renesas for the
	 * microcontroller. This change adds support for MAC address in the uC
	 * which adds extra bytes to what are read.
	 *
	 * To correctly handle the differences, we need to read an amount of
	 * data that matches the lowest common denominator, 32 bytes on uC rev
	 * <= 5.
	 *
	 * Later, if we check and find that the rev is >= 6, then we can re-read
	 * the data to get the full 38 bytes which includes the MAC address on the
	 * newer microcontroller revisions.
	 * To correctly handle errors, we need to have read 38 bytes if rev is
	 * >= 6, and 32 bytes for rev <= 5. Anything other than that should be
	 * considered a failure.
	 */
	if (v0_stream_read(i2cfd, i2cdevaddr, (uint8_t *)data, 32) < 0) {
		fprintf(stderr, "I2C Read failed\n");
		return -1;
	}

	for (i = 0; i <= 16; i++)
		data[i] = swap_byte_order(data[i]);

	printf("VDD_ARM_CAP=%d\n", sscale(data[0]));
	printf("VDD_HIGH_CAP=%d\n", sscale(data[1]));
	printf("VDD_SOC_CAP=%d\n", sscale(data[2]));
	printf("VDD_ARM=%d\n", sscale(data[3]));
	printf("P10_RAW=0x%X\n", data[4]);
	printf("P11_RAW=0x%X\n", data[5]);
	printf("P12_RAW=0x%X\n", data[6]);
	printf("VIN=%d\n", rscale(data[7], 2870, 147));
	printf("V5_A=%d\n", rscale(data[8], 147, 107));
	printf("V3P1=%d\n", rscale(data[9], 499, 499));
	printf("DDR_1P5V=%d\n", sscale(data[10]));
	printf("V1P8=%d\n", sscale(data[11]));
	printf("V1P2=%d\n", sscale(data[12]));
	printf("RAM_VREF=%d\n", sscale(data[13]));
	printf("V3P3=%d\n", rscale(data[14], 499, 499));
	printf("MICROREV=%d\n", data[15]);

	printf("P10_UA=%d\n", cscale(data[4], 110));
	printf("P11_UA=%d\n", cscale(data[5], 110));
	printf("P12_UA=%d\n", cscale(data[6], 110));
	if (data[15] >= 6) {
		if (v0_stream_read(i2cfd, i2cdevaddr, (uint8_t *)data, 38) < 0) {
			fprintf(stderr, "MAC read failed\n");
			return -1;
		}
		// MAC is stored starting at offset 16
		uint8_t *mac_bytes = (uint8_t *)&data[16];
		printf("MAC=\"%02x:%02x:%02x:%02x:%02x:%02x\"\n",
			mac_bytes[0], mac_bytes[1], mac_bytes[3],
			mac_bytes[2], mac_bytes[5], mac_bytes[4]);
	}

	return 0;
}

static unsigned char const crc8x_table[] = {
	0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15, 0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D, 0x70, 0x77,
	0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65, 0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D, 0xE0, 0xE7, 0xEE, 0xE9,
	0xFC, 0xFB, 0xF2, 0xF5, 0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD, 0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B,
	0x82, 0x85, 0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD, 0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
	0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA, 0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2, 0x8F, 0x88,
	0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A, 0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32, 0x1F, 0x18, 0x11, 0x16,
	0x03, 0x04, 0x0D, 0x0A, 0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42, 0x6F, 0x68, 0x61, 0x66, 0x73, 0x74,
	0x7D, 0x7A, 0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C, 0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
	0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC, 0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4, 0x69, 0x6E,
	0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C, 0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44, 0x19, 0x1E, 0x17, 0x10,
	0x05, 0x02, 0x0B, 0x0C, 0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34, 0x4E, 0x49, 0x40, 0x47, 0x52, 0x55,
	0x5C, 0x5B, 0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63, 0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
	0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13, 0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB, 0x96, 0x91,
	0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83, 0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB, 0xE6, 0xE1, 0xE8, 0xEF,
	0xFA, 0xFD, 0xF4, 0xF3
};

static uint8_t crc8(uint8_t *input_str, size_t num_bytes)
{
	size_t a;
	uint8_t crc = 0;
	uint8_t *ptr = input_str;

	if (ptr != NULL) {
		for (a = 0; a < num_bytes; a++) {
			crc = crc8x_table[(*ptr++) ^ crc];
		}
	}

	return crc;
}

static int do_mac(int i2cfd, char *mac_opt)
{
	int r;
	uint16_t rev;
	uint8_t mac[7];
	uint8_t buf[38];

	/* First get the uC rev, attempting to send the MAC address data
	 * to an older uC rev will cause an erroneous sleep.
	 */
	if (v0_stream_read(i2cfd, i2cdevaddr, buf, 32) < 0) {
		fprintf(stderr, "Short read of I2C\n");
		return -1;
	}

	rev = (buf[30] << 8) | buf[31];
	if (rev < 6 || model != 0x7970) {
		fprintf(stderr, "MAC cannot be set on this microcontroller!\n");
		return -1;
	}

	/* Set the MAC address */
	if (mac_opt != NULL) {
		r = sscanf(mac_opt, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
				    &mac[0], &mac[1], &mac[2], &mac[3], &mac[4],
				    &mac[5]);
		if (r != 6) {
			fprintf(stderr, "MAC address incorrectly formatted\n");
			return -1;
		}
		mac[6] = crc8(mac, 6);

		if (v0_stream_write(i2cfd, i2cdevaddr, mac, 7) < 0) {
			fprintf(stderr, "Failed writing mac over i2c");
			return -1;
		}
	}

	if (v0_stream_read(i2cfd, i2cdevaddr, buf, 38) < 0) {
		fprintf(stderr, "Short read of I2C!\n");
		return -1;
	}

	printf("MAC=\"%02x:%02x:%02x:%02x:%02x:%02x\"\n",
		buf[33], buf[32], buf[35], buf[34], buf[37], buf[36]);

	return 0;
}

static void usage(char **argv)
{
	fprintf(stderr,
		"Usage: %s [OPTION] ...\n"
		"embeddedTS Microcontroller Access\n"
		"\n"
		"  -h, --help            This message\n"
		"  -i, --info            Read all microcontroller ADC values and rev\n"
		"  -s, --sleep <seconds> Put the board in a sleep mode for n seconds\n"
		"    All values are returned in mV unless otherwise labeled\n\n",
		argv[0]);
}

int main(int argc, char **argv)
{
	int c;
	int i2cfd;
	char *macptr = NULL;
	int print_info = 0;
	int enter_sleep = 0;
	int set_mac = 0;

	static struct option long_options[] = { { "info", no_argument, 0, 'i' },
						{ "sleep", required_argument, 0, 's' },
						{ "mac", required_argument, 0, 'm' },
						{ "help", 0, 0, 'h' },
						{ 0, 0, 0, 0 } };

	if (argc == 1) {
		usage(argv);
		return 1;
	}

	model = get_model();
	if (model == 0x7970) {
		i2cdevaddr = 0x10;
	} else if (model == 0x7990) {
		i2cdevaddr = 0x4a;
	} else {
		fprintf(stderr, "Not supported on model 0x%X\n", model);
		return 1;
	}

	i2cfd = micro_init(0, i2cdevaddr);
	if (i2cfd < 0)
		return 1;

	while ((c = getopt_long(argc, argv, "is:m:h", long_options, NULL)) != -1) {
		switch (c) {
		case 'i':
			print_info = 1;
			break;
		case 's':
			enter_sleep = atoi(optarg);
			if (enter_sleep < 1) {
				fprintf(stderr, "Invalid sleep time: %d\n",
					enter_sleep);
				return 1;
			}
			break;
		case 'm':
			set_mac = 1;
			macptr = optarg;
			break;
		case 'h':
			usage(argv);
			return 0;
		default:
			usage(argv);
			return 1;
		}
	}

	if (enter_sleep)
		if (do_sleep(i2cfd, enter_sleep) < 0)
			return 1;

	if (set_mac)
		if (do_mac(i2cfd, macptr) < 0)
			return 1;

	if (print_info) {
		if (model == 0x7970) {
			if (do_ts7970_info(i2cfd) < 0)
				return 1;
		} else if (model == 0x7990) {
			if (do_ts7990_info(i2cfd) < 0)
				return 1;
		}
	}

	return 0;
}
