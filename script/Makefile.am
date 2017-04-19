SYSTEMDDIR=$(shell pkg-config systemd --variable=systemdsystemunitdir)

bin_SCRIPTS = tssilomon

EXTRA_DIST = tssilomon.service tssilomon

systemddir = $(DESTDIR)$(SYSTEMDDIR)
systemd_DATA = tssilomon.service