// This is part of Pico Library

// Most code (c) Copyright 2004 Dave, All rights reserved.
// Some code (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "teak/teak.h"
#include "PicoInt.h"

#ifdef SW_SCAN_RENDERER
#define VIDEO_OPT 0
#else
#define VIDEO_OPT 0x10
#endif

u16 PicoVer=0x0080;
struct Pico Pico;
// int PicoOpt=0xffffffff; // enable everything
// int PicoOpt=0x10; // frame-based rendering
// int PicoOpt = 0; // enable nothing, scanline rendering (broken in new ver)
// int PicoOpt=0x17; // mono sound 
// int PicoOpt=0x1f; // stereo sound
u16 PicoOpt = VIDEO_OPT;
u16 PicoSkipFrame=0; // skip rendering frame?

static u16 DefaultCram(u16 cram)
{
  u16 high=0x0841;
  // Convert 0000bbbb ggggrrrr
  // to      rrrr1ggg g10bbbb1
  high|=(cram&0x00f)<<12; // Red
  high|=(cram&0x0f0)<< 3; // Green
  high|=(cram&0xf00)>> 7; // Blue
  return high;
}

// Cram directly to BMP16 BG values for writing to an exrot BG
static u16 BGCram(u16 cram)
{
	// Convert	0000bbbbggggrrrr
	// to		1bbbb0gggg0rrrr0
	// 			0000111100000000	B	3840
	// 			0000000011110000	G	240
	// 			0000000000001111	R	15
	// and w/	1111101111011110		64478
	// or w/    1000000000000000		32768
	return 32768|(((cram&3840)<<3)|((cram&240)<<2)|((cram&15)<<1));
}

// Function to convert Megadrive Cram into a native colour:
// u16 (*PicoCram)(int cram)=DefaultCram;
u16 (*PicoCram)(u16 cram)=BGCram;
