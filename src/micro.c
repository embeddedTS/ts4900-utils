#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

int micro_init(int i2cbus, int i2caddr)
{
	static int fd = -1;
	char i2c_bus_path[20];

	if (fd != -1)
		return fd;

	snprintf(i2c_bus_path, sizeof(i2c_bus_path), "/dev/i2c-%d", i2cbus);
	fd = open(i2c_bus_path, O_RDWR);
	if (fd < 0) {
		perror("Couldn't open i2c device");
		goto out;
	}

	/*
	 * We use force because there is typically a driver attached. This is
	 * safe because we are using only i2c_msgs and not read()/write() calls
	 */
	if (ioctl(fd, I2C_SLAVE_FORCE, i2caddr) < 0) {
		perror("Supervisor did not ACK");
		close(fd);
		fd = -1;
	}

out:
	return fd;
}

/*
 * Returns < 0 on failure, 0 on success
 * Data read is inserted in to *data
 */
int speekstream16(int i2cfd, uint16_t i2caddr, uint16_t addr, uint16_t *data, uint16_t size)
{
	struct i2c_rdwr_ioctl_data packets;
	struct i2c_msg msgs[2];
	int ret;

	msgs[0].addr = i2caddr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = (uint8_t *)&addr;

	msgs[1].addr = i2caddr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = size;
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

/*
 * Returns < 0 on failure, 0 on success
 */
int spokestream16(int i2cfd, uint16_t i2caddr, uint16_t addr, uint16_t *data, uint16_t size)
{
	struct i2c_rdwr_ioctl_data packets;
	struct i2c_msg msg;
	uint8_t *outdata;
	int ret;

	/*
	 * Linux only supports 4k transactions at a time, and we need
	 * two bytes for the address
	 */
	assert(size <= 4094);
	outdata = malloc(size + 2);

	memcpy(outdata, &addr, 2);
	memcpy(&outdata[2], data, size);

	msg.addr = i2caddr;
	msg.flags = 0;
	msg.len = 2 + size;
	msg.buf = outdata;

	packets.msgs = &msg;
	packets.nmsgs = 1;

	ret = ioctl(i2cfd, I2C_RDWR, &packets);
	free(outdata);

	/* I2C_RDWR will return < 0 on error, or the number of messages that
	 * were transferred. We should always have one since we are only ever
	 * sending a single transfer.
	 */
	if (ret < 0)
		perror("Unable to send data");
	else if (ret == 1)
		ret = 0;
	else
		ret = -1;

	return ret;
}

/*
 * Since we are not expecting any data, simply pass along the return value
 * of the stream write.
 *
 * < 0 on failure, 0 on success
 */
int spoke16(int i2cfd, uint16_t i2caddr, uint16_t addr, uint16_t data)
{
	return spokestream16(i2cfd, i2caddr, addr, &data, 2);
}

/*
 * Returns < 0 on failure
 */
int speek16(int i2cfd, uint16_t i2caddr, uint16_t addr, uint16_t *data)
{
	return speekstream16(i2cfd, i2caddr, addr, data, 2);
}

/*
 * Returns 0 on success, < 0 on all other failures.
 */
static int __v0_stream(int twifd, uint16_t i2caddr, uint8_t *data, uint16_t bytes, uint16_t flags)
{
	struct i2c_rdwr_ioctl_data packets;
	struct i2c_msg msg;
	int ret = 0;

	msg.addr = i2caddr;
	msg.flags = flags;
	msg.len = bytes;
	msg.buf = data;

	packets.msgs = &msg;
	packets.nmsgs = 1;

	/* I2C_RDWR will return < 0 on error, or the number of messages that
	 * were transferred. We should always have one since we are only ever
	 * sending a single transfer.
	 */
	ret = ioctl(twifd, I2C_RDWR, &packets);
	if (ret < 0)
		perror("Unable to transfer data");
	else if (ret == 1)
		ret = 0;
	else
		ret = -1;

	return ret;
}

int v0_stream_read(int twifd, uint16_t i2caddr, uint8_t *data, uint16_t bytes)
{
	return __v0_stream(twifd, i2caddr, data, bytes, I2C_M_RD);
}

int v0_stream_write(int twifd, uint16_t i2caddr, uint8_t *data, uint16_t bytes)
{
	return __v0_stream(twifd, i2caddr, data, bytes, 0);
}
