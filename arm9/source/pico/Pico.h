
// -------------------- Pico Library --------------------

// Pico Library - Header File

// Most code (c) Copyright 2004 Dave, All rights reserved.
// Some code (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

#include <stdio.h>
#include <nds/ndstypes.h>
// #include "fat/gba_nds_fat.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

// Pico.cpp
extern int PicoVer;
extern int PicoOpt;  // bits LSb->MSb: enable_ym2612&dac, enable_sn76496, enable_z80, stereo_sound, alt_renderer, 6button_gamepad
extern int PicoSkipFrame; // skip rendering frame, but still do sound (if enabled) and emulation stuff
int PicoInit();
void PicoExit();
int PicoReset(int hard);
int PicoFrame();
extern int PicoPad[2]; // Joypads, format is MXYZ SACB RLDU
extern int (*PicoCram)(int cram); // Callback to convert colour ram 0000bbb0 ggg0rrr0

// Area.cpp
typedef size_t (arearw)(void *p, size_t _size, size_t _n, void *file);
struct PicoArea { void *data; int len; char *name; };
extern int (*PicoAcb)(struct PicoArea *); // Area callback for each block of memory
extern void *PmovFile; // this can be FILE* or gzFile
extern arearw *areaRead;  // read and write function pointers for
extern arearw *areaWrite; // gzip save state ability
extern int PmovAction;
// &1=for reading &2=for writing &4=volatile &8=non-volatile
//int PicoAreaScan(int action,int *pmin);
// Save or load the state from PmovFile:
int PmovState();
int PmovUpdate();

// Cart.c
int PicoCartLoad(FILE *f,unsigned char **prom,unsigned int *psize);
int PicoCartInsert(unsigned char *rom,unsigned int romsize);
// notaz
int CartLoadZip(const char *fname, unsigned char **prom, unsigned int *psize);
void Byteswap(unsigned char *data,int len);
int DecodeSmd(unsigned char *data,int len);

// Draw.c
extern int (*PicoScan)(unsigned int num,unsigned short *data);
//extern int PicoMask; // Mask of which layers to draw // notaz: removed because it is unused anyway

// Draw2.c
// stuff below is optional
extern unsigned short *PicoCramHigh; // pointer to CRAM buff (0x40 shorts), converted to native device color (works only with 16bit for now)
extern void (*PicoPrepareCram)();    // prepares PicoCramHigh for renderer to use

// Sek.c
extern char PicoStatus[];

// Psnd.c
//extern unsigned char PicoSreg[];

// sound.c
extern int PsndRate,PsndLen;
extern short *PsndOut;
void sound_reset(int mask);
void z80_pack(unsigned char *data);
void z80_unpack(unsigned char *data);
void z80_reset();

// Utils.c
extern int PicuAnd;
int PicuQuick(unsigned short *dest,unsigned short *src);
int PicuShrink(unsigned short *dest,int destLen,unsigned short *src,int srcLen);
int PicuShrinkReverse(unsigned short *dest,int destLen,unsigned short *src,int srcLen);
int PicuMerge(unsigned short *dest,int destLen,unsigned short *src,int srcLen);

#ifdef __cplusplus
} // End of extern "C"
#endif
