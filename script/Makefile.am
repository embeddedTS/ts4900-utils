SYSTEMDDIR=$(shell pkg-config systemd --variable=systemdsystemunitdir)

bin_SCRIPTS = tssilomon

systemddir = $(DESTDIR)$(SYSTEMDDIR)
systemd_DATA = tssilomon.service

install-data-am:
	-systemctl daemon-reload
	systemctl enable tssilomon.service

uninstall: uninstall-am
	systemctl disable tssilomon.service