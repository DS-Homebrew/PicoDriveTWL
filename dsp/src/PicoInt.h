// Pico Library - Header File

// Most code (c) Copyright 2004 Dave, All rights reserved.
// Some code (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
//#include "Pico.h"
#include "teak/teak.h"


// main oscillator clock which controls timing
#define OSC_NTSC 53693100
#define OSC_PAL  53203424

struct PicoVideo
{
  unsigned char reg[0x20];
  u32 command; // 32-bit Command
  unsigned char pending; // 1 if waiting for second half of 32-bit command
  unsigned char type; // Command type (v/c/vsram read/write)
  unsigned short addr; // Read/Write address
  s32 status; // Status bits
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
  u32 romsize;        // 0x22204

  struct PicoMisc m;
  struct PicoVideo video;
};

// Draw.c
int PicoLine(int scan);

// Draw2.c
void PicoFrameFull();

// Pico.c
extern struct Pico Pico;

