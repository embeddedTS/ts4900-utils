bin_SCRIPTS = tssilomon

install-data-am:
	mkdir -p /etc/systemd/system/
	cp tssilomon.service /etc/systemd/system/
	systemctl enable tssilomon.service
