#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "i2c-dev.h"

#define ISL12022_REG_OFF_VAL 0x21
#define ISL12022_REG_OFF_CTL 0x25
#define ISL12022_OFF_CTL_APPLY (1 << 0) /* Make value take affect now */
#define ISL12022_OFF_CTL_ADD (1 << 1) /* 1 if the value is add, 0 if subtract */
#define ISL12022_OFF_CTL_FLASH (1 << 2) /* 1 to commit to flash, 0 to just ram */

int rtc_init()
{
	static int fd = -1;
	DIR *d;
	struct dirent *dir;

	if (fd != -1)
		return fd;

	// Depending on which baseboard the TS-4900 is used on there
	// May be a different number of /dev/i2c-* devices.  This will
	// search for the name 21a0000.i2c where the name is the
	// memory address in the imx6 of the correct i2c bus.
	d = opendir("/sys/bus/i2c/devices/");
	if (d) {
		while ((dir = readdir(d)) != NULL) {
			char path[512], busname[512];
			int namefd;
			snprintf(path, 512, "/sys/bus/i2c/devices/%s/name", dir->d_name);
			namefd = open(path, O_RDONLY);
			if (namefd == -1)
				continue;
			if (read(namefd, busname, 512) == -1)
				perror("busname");
			if (strncmp(busname, "21a0000.i2c", 11) == 0) {
				snprintf(path, 512, "/dev/%s", dir->d_name);
				fd = open(path, O_RDWR);
			}
		}
		closedir(d);
	}

	if (fd != -1) {
		if (ioctl(fd, I2C_SLAVE_FORCE, 0x6f) < 0) {
			perror("FPGA did not ACK 0x6f\n");
			return -1;
		}
	}

	return fd;
}

void rtc_read(int i2cfd, uint8_t addr, void *data, uint8_t len)
{
	struct i2c_rdwr_ioctl_data packets;
	struct i2c_msg msgs[2];
	int retry = 0;

retry:
	msgs[0].addr = 0x6f;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = (char *)&addr;

	msgs[1].addr = 0x6f;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = (char *)data;

	packets.msgs = msgs;
	packets.nmsgs = 2;

	if (ioctl(i2cfd, I2C_RDWR, &packets) < 0) {
		perror("Unable to read data");
		retry++;
		if (retry < 10)
			goto retry;
	}
}

void rtc_write(int i2cfd, uint8_t addr, uint8_t data)
{
	struct i2c_rdwr_ioctl_data packets;
	struct i2c_msg msg;
	int retry = 0;
	uint8_t tmp[2];

	tmp[0] = addr;
	tmp[1] = data;

retry:
	msg.addr = 0x6f;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = (char *)tmp;

	packets.msgs = &msg;
	packets.nmsgs = 1;

	if (ioctl(i2cfd, I2C_RDWR, &packets) < 0) {
		perror("Unable to write data");
		retry++;
		if (retry < 10)
			goto retry;
	}
}

/* Return temp n millicelcius */
int rtc_temp_read(int i2cfd)
{
	uint8_t data[2];
	rtc_read(i2cfd, 0x28, data, 2);

	/* Convert from Kelvin */
	return ((data[0] | (data[1] << 8)) * 500) - 273000;
}

int bcd_to_decimal(uint8_t bcd)
{
	int dec;
	dec = ((bcd & 0xf0) >> 4) * 10 + (bcd & 0xf);
	return dec;
}

void rtc_tsv2b_read(int i2cfd, struct tm *ts)
{
	time_t now;
	uint8_t data[5];
	int year;

	rtc_read(i2cfd, 0x16, data, 5);
	time(&now);
	gmtime_r(&now, ts);

	ts->tm_sec = bcd_to_decimal(data[0] & 0x7f);
	ts->tm_min = bcd_to_decimal(data[1] & 0x7f);
	ts->tm_hour = bcd_to_decimal(data[2] & 0x3f);
	ts->tm_mday = bcd_to_decimal(data[3] & 0x3f);
	ts->tm_mon = bcd_to_decimal(data[4] & 0x1f) - 1; /* hardware is 1-12, remove 1 to match 0 numbering*/
	/* Year will always match current year! */
}

void rtc_tsb2v_read(int i2cfd, struct tm *ts)
{
	time_t now;
	uint8_t data[5];
	int year;

	rtc_read(i2cfd, 0x1b, data, 5);
	time(&now);
	gmtime_r(&now, ts);

	ts->tm_sec = bcd_to_decimal(data[0] & 0x7f);
	ts->tm_min = bcd_to_decimal(data[1] & 0x7f);
	ts->tm_hour = bcd_to_decimal(data[2] & 0x3f);
	ts->tm_mday = bcd_to_decimal(data[3] & 0x3f);
	ts->tm_mon = bcd_to_decimal(data[4] & 0x1f) - 1; /* hardware is 1-12, remove 1 to match 0 numbering*/
	/* Year will always match current year! */
}

void rtc_clear_time_stamp(int i2cfd)
{
	uint8_t data;

	rtc_read(i2cfd, 0x09, &data, 1);
	data |= (1 << 7); /* CLRTS */
	rtc_write(i2cfd, 0x09, data);
}

int rtc_is_emulated(int i2cfd)
{
	uint8_t data;

	rtc_read(i2cfd, 0x0f, &data, 1);
	return !!(data & (1 << 7));
}

void rtc_offset_set(int i2cfd, long offset)
{
	uint8_t data;
	uint32_t ppb = labs(offset);

	data = (uint8_t)(ppb >> 0);
	rtc_write(i2cfd, ISL12022_REG_OFF_VAL + 0, data);
	data = (uint8_t)(ppb >> 8);
	rtc_write(i2cfd, ISL12022_REG_OFF_VAL + 1, data);
	data = (uint8_t)(ppb >> 16);
	rtc_write(i2cfd, ISL12022_REG_OFF_VAL + 2, data);
	data = (uint8_t)(ppb >> 24);
	rtc_write(i2cfd, ISL12022_REG_OFF_VAL + 3, data);

	data = ISL12022_OFF_CTL_APPLY | ((offset > 0) ? ISL12022_OFF_CTL_ADD : 0) | ISL12022_OFF_CTL_FLASH;
	rtc_write(i2cfd, ISL12022_REG_OFF_CTL, data);
}

long rtc_offset_get(int i2cfd)
{
	uint8_t data;
	uint32_t ppb;
	long offset;

	rtc_read(i2cfd, ISL12022_REG_OFF_VAL, &ppb, (sizeof(ppb)));
	rtc_read(i2cfd, ISL12022_REG_OFF_CTL, &data, 1);

	offset = ppb;

	if ((data & ISL12022_OFF_CTL_ADD) == 0)
		offset *= -1;

	return offset;
}

int main(int argc, char **argv)
{
	int i2cfd;
	int temp;
	struct tm tsv2b, tsb2v;
	char tmbuf[64];

	i2cfd = rtc_init();
	if (i2cfd == -1)
		return 1;

	/* Set PPM value if specified */
	if (argc == 2) {
		float ppm = atof(argv[1]);
		long ppb = (long)(ppm * 1000 * -1);

		rtc_offset_set(i2cfd, ppb);
	}

	temp = rtc_temp_read(i2cfd);
	printf("rtctemp_millicelcius=%d\n", temp);

	rtc_tsv2b_read(i2cfd, &tsv2b);
	strftime(tmbuf, 64, "%m-%d %H:%M:%S", &tsv2b);
	printf("poweroff_utc_timestamp_tsv2b=\"%s\"\n", tmbuf);

	rtc_tsb2v_read(i2cfd, &tsb2v);
	strftime(tmbuf, 64, "%m-%d %H:%M:%S", &tsb2v);
	printf("poweron_utc_timestamp_tsb2v=\"%s\"\n", tmbuf);

	printf("rtc_is_emulated=%d\n", rtc_is_emulated(i2cfd));
	printf("offset_ppb=%ld\n", rtc_offset_get(i2cfd));

	rtc_clear_time_stamp(i2cfd);

	close(i2cfd);
	return 0;
}
