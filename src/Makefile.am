nvramctl_SOURCES = nvram.c nvram.h
nvramctl_CPPFLAGS = -DCTL

adc8390_SOURCES = adc8390.c adc8390.h
adc8390_CPPFLAGS = -DCTL

rtctemp_SOURCES = rtctemp.c rtctemp.h
rtctemp_CPPFLAGS = -DCTL

gpioctl_SOURCES = gpiolib.c gpiolib.h
gpioctl_CPPFLAGS = -DCTL

tshwctl_SOURCES = tshwctl.c fpga.c
tshwctl_LDFLAGS = -lm

bin_PROGRAMS = tshwctl nvramctl gpioctl rtctemp adc8390
