#pragma once

int speekstream16(int i2cfd, uint16_t i2caddr, uint16_t addr, uint16_t *data, uint16_t size);
int spokestream16(int i2cfd, uint16_t i2caddr, uint16_t addr, uint16_t *data, uint16_t size);
int micro_init(int i2cbus, uint16_t i2caddr);
int spoke16(int i2cfd, uint16_t i2caddr, uint16_t addr, uint16_t data);
int speek16(int i2cfd, uint16_t i2caddr, uint16_t addr, uint16_t *data);
int v0_stream_write(int i2cfd, uint16_t i2caddr, uint8_t *data, uint16_t bytes);
int v0_stream_read(int i2cfd, uint16_t i2caddr, uint8_t *data, uint16_t bytes);
