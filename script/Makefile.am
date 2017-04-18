#scriptsdir = $(prefix)/bin
bin_SCRIPTS = tssilomon

#systemd_unit_DATA = tssilomon.service

install-data-am:
	cp tssilomon.service /etc/systemd/system/
	systemctl enable tssilomon.service
