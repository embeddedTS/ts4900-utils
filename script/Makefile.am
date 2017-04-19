bin_SCRIPTS = tssilomon

SYSTEMDDIR=$(shell pkg-config systemd --variable=systemdsystemunitdir)

install-data-am:
	$(INSTALL) -m 0644 tssilomon.service $(SYSTEMDDIR)
	systemctl enable tssilomon.service
