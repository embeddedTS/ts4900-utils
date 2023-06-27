nvramctl_SOURCES = nvram.c nvram.h
nvramctl_CPPFLAGS = -DCTL

adc8390_SOURCES = adc8390.c adc8390.h
adc8390_CPPFLAGS = -DCTL

isl12020rtc_SOURCES = isl12020rtc.c
isl12020rtc_CPPFLAGS = -DCTL

gpioctl_SOURCES = gpiolib.c gpiolib.h
gpioctl_CPPFLAGS = -DCTL

tshwctl_SOURCES = tshwctl.c fpga.c gpiolib.c gpiolib.h
tshwctl_LDFLAGS = -lm

load_fpga_SOURCES = load_fpga-ts7970.c load_fpga-ts7990.c load_fpga.c -o load_fpga gpiolib.c ispvm.c
load_fpga_LDFLAGS = -mcpu=cortex-a9

tsmicroctl_SOURCES = tsmicroctl.c
tsmicroctl_CPPFLAGS = -DCTL

bin_PROGRAMS = tshwctl nvramctl gpioctl adc8390 load_fpga tsmicroctl isl12020rtc
