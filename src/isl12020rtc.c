#include <stdio.h>
#include <unistd.h>
#include <dirent.h> 
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "i2c-dev.h"

int rtc_init()
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

	if (fd != -1) {
		if (ioctl(fd, I2C_SLAVE_FORCE, 0x6f) < 0) {
			perror("FPGA did not ACK 0x6f\n");
			return -1;
		}
	}

	return fd;
}

void rtc_read(int twifd, uint8_t addr, uint8_t *data, uint8_t len)
{
	struct i2c_rdwr_ioctl_data packets;
	struct i2c_msg msgs[2];
	int retry = 0;

retry:
	msgs[0].addr    = 0x6f;
	msgs[0].flags   = 0;
	msgs[0].len	= 1;
	msgs[0].buf	= (char *)&addr;

	msgs[1].addr    = 0x6f;
	msgs[1].flags   = I2C_M_RD;
	msgs[1].len	= len;
	msgs[1].buf	= (char *)data;

	packets.msgs  = msgs;
	packets.nmsgs = 2;

	if (ioctl(twifd, I2C_RDWR, &packets) < 0) {
		perror("Unable to read data");
		retry++;
		if (retry < 10)
			goto retry;
	}
}

void rtc_write(int twifd, uint8_t addr, uint8_t *data, uint8_t len)
{
	struct i2c_rdwr_ioctl_data packets;
	struct i2c_msg msgs[2];
	int retry = 0;

retry:
	msgs[0].addr    = 0x6f;
	msgs[0].flags   = 0;
	msgs[0].len	= 1;
	msgs[0].buf	= (char *)&addr;

	msgs[1].addr    = 0x6f;
	msgs[1].flags   = 0;
	msgs[1].len	= len;
	msgs[1].buf	= (char *)data;

	packets.msgs  = msgs;
	packets.nmsgs = 2;

	if (ioctl(twifd, I2C_RDWR, &packets) < 0) {
		perror("Unable to read data");
		retry++;
		if (retry < 10)
			goto retry;
	}
}

/* Return temp n millicelcius */
int rtc_temp_read(int twifd)
{
	uint8_t data[2];
	rtc_read(twifd, 0x28, data, 2);

	/* Convert from Kelvin */
	return ((data[0]|(data[1]<<8))*500)-273000;
}

int bcd_to_decimal(uint8_t bcd)
{
	int dec;
	dec = ((bcd & 0xf0) >> 4) * 10 + (bcd & 0xf);
	return dec;
}

void rtc_tsv2b_read(int twifd, struct tm *ts)
{
	time_t now;
	uint8_t data[5];
	int year;

	rtc_read(twifd, 0x16, data, 5);
	time(&now);
  	gmtime_r(&now, ts);

	ts->tm_sec = bcd_to_decimal(data[0] & 0x7f);
	ts->tm_min = bcd_to_decimal(data[1] & 0x7f);
	ts->tm_hour = bcd_to_decimal(data[2] & 0x3f);
	ts->tm_mday = bcd_to_decimal(data[3] & 0x3f);
	ts->tm_mon = bcd_to_decimal(data[4] & 0x1f) - 1; /* hardware is 1-12, remove 1 to match 0 numbering*/
	/* Year will always match current year! */
}

void rtc_tsb2v_read(int twifd, struct tm *ts)
{
	time_t now;
	uint8_t data[5];
	int year;

	rtc_read(twifd, 0x1b, data, 5);
	time(&now);
  	gmtime_r(&now, ts);

	ts->tm_sec = bcd_to_decimal(data[0] & 0x7f);
	ts->tm_min = bcd_to_decimal(data[1] & 0x7f);
	ts->tm_hour = bcd_to_decimal(data[2] & 0x3f);
	ts->tm_mday = bcd_to_decimal(data[3] & 0x3f);
	ts->tm_mon = bcd_to_decimal(data[4] & 0x1f) - 1; /* hardware is 1-12, remove 1 to match 0 numbering*/
	/* Year will always match current year! */
}

void rtc_clear_time_stamp(int twifd)
{
	uint8_t data;

	rtc_read(twifd, 0x09, &data, 1);
	data |= (1 << 7); /* CLRTS */
	rtc_write(twifd, 0x09, &data, 1);
}

int main(int argc, char **argv)
{
	int twifd;
	int temp;
	struct tm tsv2b, tsb2v;
	char tmbuf[64];

	twifd = rtc_init();
	if (twifd == -1)
		return 1;

	temp = rtc_temp_read(twifd);
	printf("rtctemp_millicelcius=%d\n", temp);

	rtc_tsv2b_read(twifd, &tsv2b);
	strftime(tmbuf, 64, "%m-%d %H:%M:%S", &tsv2b);
	printf("poweroff_utc_timestamp_tsv2b=\"%s\"\n", tmbuf);

	rtc_tsb2v_read(twifd, &tsb2v);
	strftime(tmbuf, 64, "%m-%d %H:%M:%S", &tsb2v);
	printf("poweron_utc_timestamp_tsb2v=\"%s\"\n", tmbuf);

	rtc_clear_time_stamp(twifd);

	close(twifd);
	return 0;
}
