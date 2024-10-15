#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#define ISL12022_REG_OFF_VAL 0x21
#define ISL12022_REG_OFF_CTL 0x25
#define ISL12022_OFF_CTL_APPLY (1 << 0) /* Make value take affect now */
#define ISL12022_OFF_CTL_ADD (1 << 1) /* 1 if the value is add, 0 if subtract */
#define ISL12022_OFF_CTL_FLASH (1 << 2) /* 1 to commit to flash, 0 to just ram */

int rtc_init(void)
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
	if (d == NULL) {
		perror("Unable to open i2c sys directory");
		return -1;
	}

	while ((dir = readdir(d)) != NULL) {
		char path[512], busname[512];
		int namefd;
		snprintf(path, 512, "/sys/bus/i2c/devices/%s/name", dir->d_name);
		namefd = open(path, O_RDONLY);
		if (namefd == -1)
			continue;
		if (read(namefd, busname, 512) == -1) {
			perror("Unable to read from bus name file");
			fd = -1;
			break;
		}
		if (strncmp(busname, "21a0000.i2c", 11) == 0) {
			snprintf(path, 512, "/dev/%s", dir->d_name);
			fd = open(path, O_RDWR);
			if (fd < 0) {
				perror("Unable to open RTC");
				break;
			}
		}
	}
	closedir(d);

	return fd;
}

int rtc_read(int i2cfd, uint8_t addr, void *data, uint8_t len)
{
	struct i2c_rdwr_ioctl_data packets;
	struct i2c_msg msgs[2];
	int ret;

	msgs[0].addr = 0x6f;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &addr;

	msgs[1].addr = 0x6f;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = (uint8_t *)data;

	packets.msgs = msgs;
	packets.nmsgs = 2;

	/* I2C_RDWR will return < 0 on error, or the number of messages that
	 * were transferred. We should always have two since we are only ever
	 * sending a write followed by a read.
	 */
	ret = ioctl(i2cfd, I2C_RDWR, &packets);
	if (ret < 0)
		perror("Unable to read data");
	else if (ret == 2)
		ret = 0;
	else
		ret = -1;

	return ret;
}

int rtc_write(int i2cfd, uint8_t addr, uint8_t data)
{
	struct i2c_rdwr_ioctl_data packets;
	struct i2c_msg msg;
	int ret;
	uint8_t tmp[2];

	tmp[0] = addr;
	tmp[1] = data;

	msg.addr = 0x6f;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = tmp;

	packets.msgs = &msg;
	packets.nmsgs = 1;

	/* I2C_RDWR will return < 0 on error, or the number of messages that
	 * were transferred. We should always have one since we are only ever
         * sending a single transfer.
	 */
	ret = ioctl(i2cfd, I2C_RDWR, &packets);
	if (ret < 0)
		perror("Unable to read data");
	else if (ret == 1)
		ret = 0;
	else
		ret = -1;

	return ret;
}

/* Return temp n millicelcius */
int rtc_temp_read(int i2cfd, int *mc)
{
	uint8_t data[2];

	if (rtc_read(i2cfd, 0x28, data, 2) < 0)
		return -1;

	/* Convert from Kelvin to millicelsius */
	*mc = ((data[0] | (data[1] << 8)) * 500) - 273000;

	return 0;
}

int bcd_to_decimal(uint8_t bcd)
{
	int dec;
	dec = ((bcd & 0xf0) >> 4) * 10 + (bcd & 0xf);
	return dec;
}

int rtc_tsv2b_read(int i2cfd, struct tm *ts)
{
	time_t now;
	uint8_t data[5];

	if (rtc_read(i2cfd, 0x16, data, 5) < 0)
		return -1;
	time(&now);
	gmtime_r(&now, ts);

	ts->tm_sec = bcd_to_decimal(data[0] & 0x7f);
	ts->tm_min = bcd_to_decimal(data[1] & 0x7f);
	ts->tm_hour = bcd_to_decimal(data[2] & 0x3f);
	ts->tm_mday = bcd_to_decimal(data[3] & 0x3f);
	ts->tm_mon = bcd_to_decimal(data[4] & 0x1f) - 1; /* hardware is 1-12, remove 1 to match 0 numbering*/
	/* Year will always match current year! */

	return 0;
}

int rtc_tsb2v_read(int i2cfd, struct tm *ts)
{
	time_t now;
	uint8_t data[5];

	if (rtc_read(i2cfd, 0x1b, data, 5) < 0)
		return -1;
	time(&now);
	gmtime_r(&now, ts);

	ts->tm_sec = bcd_to_decimal(data[0] & 0x7f);
	ts->tm_min = bcd_to_decimal(data[1] & 0x7f);
	ts->tm_hour = bcd_to_decimal(data[2] & 0x3f);
	ts->tm_mday = bcd_to_decimal(data[3] & 0x3f);
	ts->tm_mon = bcd_to_decimal(data[4] & 0x1f) - 1; /* hardware is 1-12, remove 1 to match 0 numbering*/
	/* Year will always match current year! */

	return 0;
}

int rtc_clear_time_stamp(int i2cfd)
{
	uint8_t data;

	if (rtc_read(i2cfd, 0x09, &data, 1) < 0) {
		fprintf(stderr, "Unable to read timestamp\n");
		return -1;
	}

	data |= (1 << 7); /* CLRTS */
	if (rtc_write(i2cfd, 0x09, data) < 0) {
		fprintf(stderr, "Unable to clear timestamp\n");
		return -1;
	}

	return 0;
}

int rtc_is_emulated(int i2cfd, int *emulated)
{
	uint8_t data;

	if (rtc_read(i2cfd, 0x0f, &data, 1) < 0)
		return -1;

	*emulated = !!(data & (1 << 7));
	return 0;
}

int rtc_offset_set(int i2cfd, long offset)
{
	uint8_t data;
	uint32_t ppb = labs(offset);
	int i;

	/* Write the ppb to the associated offset registers. This is written
	 * LSB first.
	 *
	 * TODO: Rather than looping on a byte write, it would likely be possible
	 * to modify the rtc_write() function to accept a number of bytes to
	 * transmit.
	 */
	for (i = 0; i < 4; i++) {
		data = (uint8_t)((ppb >> (i*8)) & 0xff);
		if (rtc_write(i2cfd, ISL12022_REG_OFF_VAL + i, data) < 0)
			return -1;
	}

	data = ISL12022_OFF_CTL_APPLY |
	       ((offset > 0) ? ISL12022_OFF_CTL_ADD : 0) |
	       ISL12022_OFF_CTL_FLASH;

	if (rtc_write(i2cfd, ISL12022_REG_OFF_CTL, data) < 0)
			return -1;

	return 0;
}

int rtc_offset_get(int i2cfd, long *offset)
{
	uint8_t data;
	uint32_t ppb;

	if (rtc_read(i2cfd, ISL12022_REG_OFF_VAL, &ppb, (sizeof(ppb))) < 0)
		return -1;
	if (rtc_read(i2cfd, ISL12022_REG_OFF_CTL, &data, 1) < 0)
		return -1;

	*offset = ppb;

	if ((data & ISL12022_OFF_CTL_ADD) == 0)
		*offset *= -1;

	return 0;
}

int main(int argc, char **argv)
{
	int i2cfd;
	int temp;
	struct tm tsv2b, tsb2v;
	char tmbuf[64];
	long offset;
	int emulated;
	int ret = 1;

	i2cfd = rtc_init();
	if (i2cfd == -1)
		return 1;

	/* Set PPM value if specified */
	if (argc == 2) {
		float ppm = atof(argv[1]);
		long ppb = (long)(ppm * 1000 * -1);

		if (rtc_offset_set(i2cfd, ppb) < 0)
			goto out;
	}

	if (rtc_temp_read(i2cfd, &temp) < 0)
		goto out;
	printf("rtctemp_millicelcius=%d\n", temp);

	if (rtc_tsv2b_read(i2cfd, &tsv2b) < 0)
		goto out;
	strftime(tmbuf, 64, "%m-%d %H:%M:%S", &tsv2b);
	printf("poweroff_utc_timestamp_tsv2b=\"%s\"\n", tmbuf);

	if (rtc_tsb2v_read(i2cfd, &tsb2v) < 0)
		goto out;
	strftime(tmbuf, 64, "%m-%d %H:%M:%S", &tsb2v);
	printf("poweron_utc_timestamp_tsb2v=\"%s\"\n", tmbuf);

	if (rtc_is_emulated(i2cfd, &emulated) < 0)
		goto out;
	printf("rtc_is_emulated=%d\n", emulated);

	if (rtc_offset_get(i2cfd, &offset) < 0)
		goto out;
	printf("offset_ppb=%ld\n", offset);

	if (rtc_clear_time_stamp(i2cfd) < 0)
		goto out;

	ret = 0;
out:
	close(i2cfd);
	return ret;
}
