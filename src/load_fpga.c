#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "ispvm.h"
#include "load_fpga-ts7970.h"
#include "load_fpga-ts7990.h"

char *get_model()
{
	FILE *proc;
	char mdl[256];
	char *ptr;
	int sz;

	proc = fopen("/proc/device-tree/model", "r");
	if (!proc) {
	    perror("model");
	    return 0;
	}
	sz = fread(mdl, 256, 1, proc);
	ptr = strstr(mdl, "TS-");
	return strndup(ptr, sz - (mdl - ptr));
}

int main(int argc, char **argv)
{
	int x;
	char *model = 0;
	struct ispvm_f hardware;

	const char * ispvmerr[] = { "pass", "verification fail",
	  "can't find the file", "wrong file type", "file error",
	  "option error", "crc verification error" };

	if(argc != 2) {
		printf("Usage: %s file.vme\n", argv[0]);
		return 1;
	}

	model = get_model();
	fprintf(stderr, "Model: \"%s\"\n", model);
	if(strstr(model, "7970)")) {
		hardware.init = init_ts7970;
		hardware.restore =restore_ts7970;
		hardware.readport = readport_ts7970;
		hardware.writeport = writeport_ts7970;
		hardware.sclock = sclock_ts7970;
		hardware.udelay = 0;
	} else if(strstr(model, "TPC-7990")) {
		hardware.init = init_ts7990;
		hardware.restore =restore_ts7990;
		hardware.readport = readport_ts7990;
		hardware.writeport = writeport_ts7990;
		hardware.sclock = sclock_ts7990;
		hardware.udelay = 0;
	} else {
		printf("Model \"%s\" not supported\n", model);
		return 1;
	}

	x = ispVM(&hardware, argv[1]);

	if (x == 0) {
		printf("loadfpga_ok=1\n");
	} else {
		assert(x < 0);
		printf("loadfpga_ok=0\n");
		printf("loadfpga_error=\"%s\"\n", ispvmerr[-x]);
	}

	return 0;
}
