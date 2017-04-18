bin_SCRIPTS = tssilomon

install-data-am:
	mkdir -p $(DESTDIR)/etc/systemd/system/
	$(INSTALL) tssilomon.service $(DESTDIR)/etc/systemd/system/
	systemctl enable tssilomon.service
