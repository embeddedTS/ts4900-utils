#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "i2c-dev.h"

#define I2C_ADDR 0x10
#define DEV_SIZE 129

/* Read-back status values */
/* Default value of status, locked */
/* This will never be read back, however */
#define STATUS_LOCKED       0x00
/* Once the flashwrite process is unlocked, but no data written */
#define STATUS_UNLOCKED     0xAA
/* Flashwrite process has seen full length of data written and is considered done */
#define STATUS_DONE         0x01
/* Flashwrite is in process, meaning SOME data has been written, but not the full length */
#define STATUS_IN_PROC      0x02
/* A CRC error occurred at ANY point during data write. Note that this status
 * is not set if CRC fails for unlock process, the system simply does not unlock
 */
#define STATUS_CRC_ERR      0x03
/* An error occurred while trying to erase the actual flash */
#define STATUS_ERASE_ERR    0x04
/* An error occurred at ANY point during data write. */
#define STATUS_WRITE_ERR    0x05
/* Erase was successful, but, the area to be written was not blank */
#define STATUS_NOT_BLANK    0x06
/* A BSP error opening and closing flash. Most errors are buggy code, configurations, or unrecoverable */
#define STATUS_OPEN_ERR	    0x07
/* Wait state while processing a write */
#define STATUS_WAIT         0x08
/* Request the uC reboot at any time after its unlocked status */
#define STATUS_RESET        0x55

/* Pack the struct to be sure it is only as large as we need */
struct unlock_header {
	uint32_t magic_key;
	uint32_t loc;
	uint32_t len;
	uint8_t crc;
} __attribute__((packed));

struct micro_update_footer {
        uint32_t bin_size;
        uint8_t revision;
        uint8_t flags;
        uint8_t misc;
        uint8_t footer_version;
        uint8_t magic[11];
} __attribute__((__packed__));

static unsigned char const crc8x_table[] = {
	0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
	0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
	0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
	0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
	0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
	0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
	0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
	0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
	0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
	0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
	0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
	0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
	0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
	0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
	0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
	0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
	0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
	0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
	0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
	0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
	0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
	0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
	0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
	0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
	0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
	0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
	0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
	0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
	0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
	0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
	0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
	0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

int micro_stream_read(int twifd, uint8_t *data, int bytes)
{
	struct i2c_rdwr_ioctl_data packets;
	struct i2c_msg msg;
	int retry = 0;

retry:
	msg.addr = I2C_ADDR;
	msg.flags = I2C_M_RD;
	msg.len	= bytes;
	msg.buf	= (char *)data;

	packets.msgs  = &msg;
	packets.nmsgs = 1;

	if (ioctl(twifd, I2C_RDWR, &packets) < 0) {
		perror("Unable to read data");
		retry++;
		if(retry < 10)
			goto retry;
	}
	return 0;
}

int micro_stream_write(int twifd, uint8_t *data, int bytes)
{
	struct i2c_rdwr_ioctl_data packets;
	struct i2c_msg msg;

	msg.addr = I2C_ADDR;
	msg.flags = 0;
	msg.len	= bytes;
	msg.buf	= (char *)data;

	packets.msgs  = &msg;
	packets.nmsgs = 1;

	if (ioctl(twifd, I2C_RDWR, &packets) < 0) {
		perror("Unable to send data");
		return 1;
	}
	return 0;
}

int microcontroller_revision(int twifd)
{
	uint8_t buf[32];
	int r;
	int rev;

	/* First get the uC rev, attempting to send the MAC address data
	 * to an older uC rev will cause an erroneous sleep.
	 */
	r = micro_stream_read(twifd, buf, 32);
	if (r) {
		printf("i2c read failed!\n");
		exit(1);
	}
	rev = (buf[30] << 8) | buf[31];
	return rev;
}
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

uint8_t crc8(uint8_t *input_str, size_t num_bytes)
{
	size_t a;
	uint8_t crc = 0;
	uint8_t *ptr = input_str;

	if ( ptr != NULL ) {
		for (a=0; a<num_bytes; a++) {
			crc = crc8x_table[(*ptr++) ^ crc];
		}
	}

	return crc;
}

void usage(char **argv)
{
	fprintf(stderr, "Usage: %s update.bin\n"
          "Update embeddedTS microcontrollers\n",
	argv[0]);
}

int32_t main(int argc, char **argv)
{
	char *model;
	uint8_t buf[129];
	int i2c_fd, bin_fd;
	struct stat bin_stat;
	struct unlock_header hdr;
	struct micro_update_footer ftr;
	uint8_t revision;
	int i;
	int ret;

	if (argc != 2) {
		usage(argv);
		return 1;
	}

	/* Open file */
	bin_fd = open(argv[1], O_RDONLY);
	if (bin_fd < 0)
		error(1, errno, "Error opening binary file");

	lseek(bin_fd, -(sizeof(ftr)), SEEK_END);
	ret = read(bin_fd, &ftr, sizeof(ftr));
	if(ret != sizeof(ftr))
		error(1, 0, "footer read failed!");

	if(strncmp("TS_UC_RA4M2", (char *)&ftr.magic, 11) != 0)
		error(1, 1, "Invalid update file");

	if(ftr.bin_size == 0 || ftr.bin_size > 128*1024)
		error(1, 1, "Bin size is incorrect");

	/* Check file is 128-byte aligned */
	if (ftr.bin_size & 0x7F)
		error(1, 0, "Binary file must be 128-byte aligned!");

	i2c_fd = open("/dev/i2c-0", O_RDWR);
	if (i2c_fd < 0)
		error(1, errno, "Failed to open I2C device");
		
	revision = microcontroller_revision(i2c_fd);

	if (revision < 7) {
		fprintf(stderr, "The microconroller must be rev 7 or later to support updates.\n");
		return 1;
	}
	
	model = get_model();
	if (!strstr(model, "7970")) {
		fprintf(stderr, "This update is only supported on the TS-7970\n");
		return 1;
	}

	if (ioctl(i2c_fd, I2C_SLAVE, I2C_ADDR) < 0) {
		fprintf(stderr, "Failed to set address. Already in use?\n");
		return 1;
	}

	if(revision == ftr.revision) {
		printf("Already running FPGA revision %d, not updating\n", revision);
		return 1;
	}

	printf("Updating from revision %d to %d\n", revision, ftr.revision);

	lseek(bin_fd, 0, SEEK_SET);

	hdr.magic_key = 0xf092c858;
	hdr.loc = 0x28000;
	hdr.len = ftr.bin_size;
	hdr.crc = crc8((uint8_t *)&hdr, (sizeof(struct unlock_header) - 1));

	/* Write magic key and length/location information */
	if (micro_stream_write(i2c_fd, (uint8_t *)&hdr, 13) < 0)
		error(1, errno, "Failed to write header to I2C");

	/* Wait a bit, the flash needs to open, erase, and blank check.
	 * Could also loop on I2C read for STATUS_UNLOCKED to be set */
	usleep(1000000);

	micro_stream_read(i2c_fd, buf, 1);
	if (buf[0] != STATUS_UNLOCKED)
		error(1, 0, "Device failed to report as unlocked, aborting!");

	printf("\n");
	/* Write BIN to MCU via I2C */
	for (i = ftr.bin_size; i; i -= 128) {
		printf("\r%d/%d", ftr.bin_size - i, ftr.bin_size);
		fflush(stdout);
		ret = read(bin_fd, buf, 128);
		if (ret < 0) {
			error(1, errno, "Error reading from BIN @ %d", ftr.bin_size - i);
		} else if (ret < 128) {
			error(1, 0, "Short read from BIN! Aborting!");
		} else {
			buf[128] = crc8(buf, 128);
			ret = micro_stream_write(i2c_fd, buf, 129);
			if (ret)
				error(1, errno, "Failed to write BIN to I2C @ %d (did uC I2C timeout?)", ftr.bin_size - i);

			/* There is some unknown amount of time for a write to complete, its based
			 * on the current uC clocks and all of that, but 10 microseconds should be
			 * enough in most cases */
			usleep(10);
			do {
				ret = micro_stream_read(i2c_fd, buf, 1);

				if(ret == 1)
					buf[0] = STATUS_WAIT;
				usleep(1000*5);
			} while (buf[0] == STATUS_WAIT);

			if ((buf[0] != STATUS_IN_PROC) && (buf[0] != STATUS_DONE))
				error(1, 0, "Device reported error status 0x%02X\n", buf[0]);
		}
	}
	printf("\n");

	micro_stream_read(i2c_fd, buf, 1);
	buf[1] = STATUS_RESET;
	if (buf[0] == STATUS_DONE) {
		printf("Update successful, rebooting uC\n");
	} else {
		printf("Update incomplete but not errored, rebooting uC\n");
	}
	/* Give time for the message to go to the console */
	fflush(stdout);
	sleep(1);
	/* Provoke microcontroller reset */
	micro_stream_write(i2c_fd, &buf[1], 1);
	sleep(1);
	/* If we're returning at all, something has gone wrong */
	return 1;
}
