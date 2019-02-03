// Pico Library - Header File

// Most code (c) Copyright 2004 Dave, All rights reserved.
// Some code (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Pico.h"


// test
//#ifdef ARM
//#include "../clibc/faststr.h"
//#endif

#if defined(__MARM__) || defined(_WIN32_WCE) || defined(ARM9)
#define EMU_C68K // Use the Cyclone 68000 emulator
#else
#define EMU_A68K // Use the 'A68K' (Make68K) Assembler 68000 emulator
#endif

#if defined(ARM) || defined(GP32) || defined (__MARM__) || defined(ARM9)
#define CPU_CALL
#else
#define CPU_CALL __fastcall
#endif

#ifdef __cplusplus
extern "C" {
#endif


// ----------------------- 68000 CPU -----------------------
#ifdef EMU_A68K
// The format of the data in a68k.asm (at the _M68000_regs location)
struct A68KContext
{
  unsigned int d[8],a[8];
  unsigned int isp,srh,ccr,xc,pc,irq,sr;
  int (*IrqCallback) (int nIrq);
  unsigned int ppc;
  void *pResetCallback;
  unsigned int sfc,dfc,usp,vbr;
  unsigned int AsmBank,CpuVersion;
};
struct A68KContext M68000_regs;
extern int m68k_ICount;
#define SekCycles m68k_ICount // cycles left for this line
#endif

#ifdef EMU_C68K
#include "cyclone/Cyclone.h"
extern struct Cyclone PicoCpu;
#define SekCycles PicoCpu.cycles
#endif

// ---------------------------------------------------------

// main oscillator clock which controls timing
#define OSC_NTSC 53693100
#define OSC_PAL  53203424

struct PicoVideo
{
  unsigned char reg[0x20];
  unsigned int command; // 32-bit Command
  unsigned char pending; // 1 if waiting for second half of 32-bit command
  unsigned char type; // Command type (v/c/vsram read/write)
  unsigned short addr; // Read/Write address
  int status; // Status bits
  unsigned char pad[0x14];
};

struct PicoMisc
{
  unsigned char rotate;
  unsigned char z80Run;
  unsigned char padTHPhase[2]; // phase of gamepad TH switches
  short scanline; // -38||-88 to 223
  char dirtyPal; // Is the palette dirty (currently unused)
  unsigned char hardware; // Hardware value for country
  unsigned char pal; // 1=PAL 0=NTSC
  unsigned char sram_reg; // notaz: SRAM mode register. bit0: allow read? bit1: deny write?
  unsigned short z80_bank68k;
  unsigned short z80_lastaddr; // this is for Z80 faking
  unsigned short z80_fakeval;
  unsigned char padDelay[2];  // gamepad phase time outs, so we count a delay
  unsigned char sram_changed;
  unsigned char pad1[0xd];
};

// some assembly stuff depend on these, do not touch!
struct Pico
{
  unsigned char ram[0x10000];  // 0x00000 scratch ram
  unsigned short vram[0x8000]; // 0x10000
  unsigned char zram[0x2000];  // 0x20000 Z80 ram
  unsigned char ioports[0x10];
  unsigned int pad[0x3c];      // unused
  unsigned short cram[0x40];   // 0x22100
  unsigned short vsram[0x40];  // 0x22180

  unsigned char *rom;          // 0x22200
  unsigned int romsize;        // 0x22204

  struct PicoMisc m;
  struct PicoVideo video;
};

// notaz: sram
struct PicoSRAM
{
  unsigned char *data; // actual data
  unsigned int start;  // start address in 68k address space
  unsigned int end;
  unsigned char resize; // 1=SRAM size changed and needs to be reallocated on PicoReset
  unsigned char pad[3];
};

// Draw.c
int PicoLine(int scan);

// Draw2.c
void PicoFrameFull();

// Memory.c
int PicoInitPc(unsigned int pc);
unsigned short CPU_CALL PicoRead16(unsigned int a);
unsigned int CPU_CALL PicoRead32(unsigned int a);
int PicoMemInit();
void PicoDasm(int start,int len);
unsigned char z80_read(unsigned short a);
unsigned short z80_read16(unsigned short a);
void z80_write(unsigned char data, unsigned short a);
void z80_write16(unsigned short data, unsigned short a);

// Pico.c
extern struct Pico Pico;
extern struct PicoSRAM SRam;

// Sek.c
int SekInit();
int SekReset();
int SekRun(int cyc);
int SekInterrupt(int irq);
int SekPc();
void SekState(unsigned char *data);

// Sine.c
//extern short Sine[];

// Psnd.c
//int PsndReset();
//int PsndFm(int a,int d);
//int PsndRender();

// VideoPort.c
void PicoVideoWrite(unsigned int a,unsigned int d);
unsigned int PicoVideoRead(unsigned int a);

#ifdef __DEBUG_PRINT
// External:
//int dprintf(char *Format, ...);
void dprintf(char *format, ...);
#endif

#ifdef __cplusplus
} // End of extern "C"
#endif
