#ifndef _ISPVM_H_
#define _ISPVM_H_

#define g_ucPinTDI 		0x1
#define g_ucPinTCK 		0x2
#define g_ucPinTMS 		0x4
#define g_ucPinTDO 		0x8
#define g_ucPinENABLE 	0x10
#define g_ucPinTRST 	0x20

struct ispvm_f {
	void (*init)(void);
	void (*restore)(void);
	int (*readport)(void);
	void (*writeport)(int, int);
	void (*sclock)(void);
	void (*udelay)(unsigned int us);
};

signed char ispVM( struct ispvm_f *callbacks, const char * a_pszFilename );

#endif
