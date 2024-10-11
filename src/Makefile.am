AM_CFLAGS = -Wall

isl12020rtc_SOURCES = isl12020rtc.c
isl12020rtc_CPPFLAGS = -DCTL
isl12020rtc_LDADD =

tshwctl_SOURCES = tshwctl.c fpga.c
tshwctl_CPPFLAGS = $(LIBGPIOD_CFLAGS)
tshwctl_LDADD = $(LIBGPIOD_LIBS) -lm

tsmicroctl_SOURCES = tsmicroctl.c micro.c

bin_PROGRAMS = tshwctl tsmicroctl isl12020rtc
