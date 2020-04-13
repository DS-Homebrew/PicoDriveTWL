// This is part of Pico Library

// Most code (c) Copyright 2004 Dave, All rights reserved.
// Some code (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "PicoInt.h"
#include "sound/sound.h"

#include "Timer.h"
#ifdef PROFILE
#include <nds/arm9/console.h>
#include <nds.h>
#endif

#ifdef ARM9_SOUND
#define SOUND_OPT 0xf
#else
#define SOUND_OPT 0
#endif

#ifdef SW_SCAN_RENDERER
#define VIDEO_OPT 0
#else
#define VIDEO_OPT 0x10
#endif

#ifdef SW_SCAN_RENDERER
void PicoFrameFull() {}
#endif

int PicoVer=0x0080;
struct Pico Pico;
// int PicoOpt=0xffffffff; // enable everything
// int PicoOpt=0x10; // frame-based rendering
// int PicoOpt = 0; // enable nothing, scanline rendering (broken in new ver)
// int PicoOpt=0x17; // mono sound 
// int PicoOpt=0x1f; // stereo sound
int PicoOpt = VIDEO_OPT | SOUND_OPT;
int PicoSkipFrame=0; // skip rendering frame?

// notaz: sram
struct PicoSRAM SRam;

int PicoPad[2]; // Joypads, format is SACB RLDU

// to be called once on emu init
int PicoInit()
{
  // Blank space for state:
  memset(&Pico,0,sizeof(Pico));
  memset(&PicoPad,0,sizeof(PicoPad));

  // Init CPUs:
  SekInit();
#ifdef ARM9_SOUND
  z80_init(); // init even if we aren't going to use it
#endif

  // Setup memory callbacks:
  PicoMemInit();

  // notaz: sram
  SRam.data=0;
  SRam.resize=1;

  return 0;
}

// to be called once on emu exit
void PicoExit()
{
#ifdef ARM9_SOUND
  z80_exit();
#endif

  // notaz: sram
  if(SRam.data) free(SRam.data); SRam.data=0;
}

int PicoReset(int hard)
{
  unsigned int region=0;
  int support=0,hw=0,i=0;
  unsigned char pal=0;

#ifdef PROFILE  
  Timer_Init();
#endif
  
  if (Pico.romsize<=0) return 1;

  SekReset();
#ifdef ARM9_SOUND
  z80_reset();
#endif

  // reset VDP state, VRAM and PicoMisc 
  memset(&Pico.video,0,sizeof(Pico.video));
  memset(&Pico.vram,0,sizeof(Pico.vram));
  memset(&Pico.m,0,sizeof(Pico.m));

  if(hard) {
    // clear all memory of the emulated machine
    memset(&Pico.ram,0,(unsigned int)&Pico.rom-(unsigned int)&Pico.ram);
  }

  // Read cartridge region data:
  region=PicoRead32(0x1f0);

  for (i=0;i<4;i++)
  {
    int c=0;
    
    c=region>>(i<<3); c&=0xff;
    if (c<=' ') continue;

         if (c=='J') support|=1;
    else if (c=='U') support|=4;
    else if (c=='E') support|=8;
    else
    {
      // New style code:
      char s[2]={0,0};
      s[0]=(char)c;
      support|=strtol(s,NULL,16);
    }
  }

  // Try to pick the best hardware value for English/50hz:
       if (support&8) { hw=0xc0; pal=1; } // Europe
  else if (support&4)   hw=0x80;          // USA
  else if (support&2) { hw=0x40; pal=1; } // Japan PAL
  else if (support&1)   hw=0x00;          // Japan NTSC
  else hw=0x80; // USA

  Pico.m.hardware=(unsigned char)(hw|0x20); // No disk attached
  Pico.m.pal=pal;
  Pico.video.status &= ~1;
  Pico.video.status |= pal;

#ifdef ARM9_SOUND
  sound_reset(PicoOpt);
#endif

  // notaz: sram
  if(SRam.resize) {
    int sram_size = 0;
    if(*(Pico.rom+0x1B1) == 'R' && *(Pico.rom+0x1B0) == 'A' && (*(Pico.rom+0x1B3) & 0x40)) {
      SRam.start = PicoRead32(0x1B4) & 0xFFFF00;
      SRam.end   = PicoRead32(0x1B8);
      sram_size = SRam.end - SRam.start + 1;
    }
    
    if(sram_size <= 0 || sram_size >= 0x40000) {
      SRam.start = 0x200000;
      SRam.end   = 0x203FFF;
      sram_size  = 0x004000;
    }
    
	if(SRam.data) free(SRam.data);
    SRam.data = (unsigned char *) calloc(sram_size, 1);
    if(!SRam.data) return 1;
	SRam.resize=0;
  }

  // enable sram access by default if it doesn't overlap with ROM
  if(Pico.romsize <= SRam.start) Pico.m.sram_reg = 1;
  
  for (int i = 1; i <= 7; i++) {
	Pico.m.romBank[i-1] = i;
  }

  return 0;
}

static int CheckIdle()
{
#if 1
  unsigned char state[0x88];

  memset(state,0,sizeof(state));

  // See if the state is the same after 2 steps:
  SekState(state); SekRun(0); SekRun(0); SekState(state+0x44);
  if (memcmp(state,state+0x44,0x44)==0) return 1;
#else
  unsigned char state[0x44];
  static unsigned char oldstate[0x44];

  SekState(state);
  if(memcmp(state,oldstate,0x40)==0) return 1;
  memcpy(oldstate, state, 0x40);
#endif

  return 0;
}

// Accurate but slower frame which does hints
static int PicoFrameHints()
{
  struct PicoVideo *pv=&Pico.video;
  int total=0,total_z80=0,aim=0,aim_z80=0,cycles_68k,cycles_z80,loop_from;
  int y=0;
  int hint=0x400; // Hint counter

  if(Pico.m.pal) {
    cycles_68k = (int) ((double) OSC_PAL  /  7 / 50 / 312 + 0.4); // should compile to a constant
	cycles_z80 = (int) ((double) OSC_PAL  / 15 / 50 / 312 + 0.4);
	loop_from  = -88;
  } else {
    cycles_68k = (int) ((double) OSC_NTSC /  7 / 60 / 262 + 0.4); // 488
	cycles_z80 = (int) ((double) OSC_NTSC / 15 / 60 / 262 + 0.4); // 228
	loop_from  = -38;
  }

  pv->status|=0x08; // Go into vblank

#ifdef ARM9_SOUND
  if(PicoOpt&4)
    z80_resetCycles();
#endif

  for (y=loop_from;y<224;y++)
  {
	// pad delay (for 6 button pads)
	if(PicoOpt&0x20) {
	  if(Pico.m.padDelay[0]++ > 25) Pico.m.padTHPhase[0]=0;
	  //if(Pico.m.padDelay[1]++ > 25) Pico.m.padTHPhase[1]=0;
	}

    if (y==0)
    {
      hint=pv->reg[10]; // Load H-Int counter
      if (pv->reg[1]&0x40) pv->status&=~8; // Come out of vblank if display enabled
      SekInterrupt(0); // Cancel v interrupt
      pv->status&=~0x80;
	  // draw a frame just after vblank in anternative render mode
	  if(!PicoSkipFrame && (PicoOpt&0x10))
	    PicoFrameFull();
    }

    aim+=cycles_68k;
    // run for a little while in hblank state
	//pv->status|=4;
    //total+=SekRun(aim-total-404);
	//pv->status&=~4;

    // H-Interrupts:
    if (hint < 0)
    {
      hint=pv->reg[10]; // Reload H-Int counter
      if (pv->reg[0]&0x10) SekInterrupt(4);
    }

    // V-Interrupt:
    if (y == loop_from+1) // == loop_from ?
    {
      pv->status|=0x80; // V-Int happened
      if(pv->reg[1]&0x20) SekInterrupt(6);
#ifdef ARM9_SOUND
	  if(Pico.m.z80Run && (PicoOpt&4)) z80_int();
#endif
    }

    Pico.m.scanline=(short)y;

    // Run scanline:
    total+=SekRun(aim-total);
#ifdef ARM9_SOUND
    if((PicoOpt&4) && Pico.m.z80Run) {
      aim_z80+=cycles_z80;
      total_z80+=z80_run(aim_z80-total_z80);
	}

	if(PicoOpt&1)
      sound_timers_and_dac(y-loop_from);
#endif

    // even if we are in PAL mode, we draw 224 lines only (although we sould do 240 in some cases)
    if(y>=0 && !PicoSkipFrame && !(PicoOpt&0x10)) PicoLine(y);

	hint--;
  }

  return 0;
}

// Simple frame without H-Ints
static int PicoFrameSimple()
{
  int total=0,total_z80=0,aim=0,aim_z80=0,y=0,z80lines=0,z80line=0,sects;
  int cycles_68k_vblank,cycles_z80_vblank,cycles_68k_block,cycles_z80_block,cycles_z80_line;

  if(Pico.m.pal) {
    cycles_68k_vblank = (int) ((double) OSC_PAL  /  7 / 50 / 312 + 0.4) * 88;
	cycles_z80_vblank = (int) ((double) OSC_PAL  / 15 / 50 / 312 + 0.4) * 88;
    cycles_68k_block  = (int) ((double) OSC_PAL  /  7 / 50 / 312 + 0.4) * 14;
	cycles_z80_block  = (int) ((double) OSC_PAL  / 15 / 50 / 312 + 0.4) * 14;
	cycles_z80_line   = (int) ((double) OSC_PAL  / 15 / 50 / 312 + 0.4);
	z80lines = 88;
  } else {
    cycles_68k_vblank = (int) ((double) OSC_NTSC /  7 / 60 / 262 + 0.4) * 38; // 7790
	cycles_z80_vblank = (int) ((double) OSC_NTSC / 15 / 60 / 262 + 0.4) * 38;
    cycles_68k_block  = (int) ((double) OSC_NTSC /  7 / 60 / 262 + 0.4) * 14;
	cycles_z80_block  = (int) ((double) OSC_NTSC / 15 / 60 / 262 + 0.4) * 14;
	cycles_z80_line   = (int) ((double) OSC_NTSC / 15 / 60 / 262 + 0.4); // 228
	z80lines = 38;
  }
 
  /* 
  cycles_68k_vblank = (int) ((double) OSC_NTSC /  7 / 60 / 262 + 0.4) * 19; // 7790
  cycles_z80_vblank = (int) ((double) OSC_NTSC / 15 / 60 / 262 + 0.4) * 19;
  cycles_68k_block  = (int) ((double) OSC_NTSC /  7 / 60 / 262 + 0.4) * 7;
  cycles_z80_block  = (int) ((double) OSC_NTSC / 15 / 60 / 262 + 0.4) * 7;
  cycles_z80_line   = (int) ((double) OSC_NTSC / 15 / 60 / 262 + 0.4); // 228
  z80lines = 38;
  */

  Pico.m.scanline=-100;

#ifdef ARM9_SOUND
  if(PicoOpt&4)
    z80_resetCycles();
#endif

  // V-Blanking period:
  if (Pico.video.reg[1]&0x20) SekInterrupt(6); // Set IRQ
  Pico.video.status|=0x88; // V-Int happened / go into vblank
  
  // SetupProfile();
  
  // BeginProfile();
  total=SekRun(cycles_68k_vblank);
  
#ifdef ARM9_SOUND
  if((PicoOpt&4) && Pico.m.z80Run) {
    z80_int();
	if(PicoOpt&1) {
	  // we have ym2612 enabled, so we have to run Z80 in lines, so we could update DAC and timers
	  for(; z80line < z80lines; z80line++) {
	    aim_z80+=cycles_z80_line;
        total_z80+=z80_run(aim_z80-total_z80);
	    sound_timers_and_dac(z80line);
	  }
	} else {
	    aim_z80=cycles_z80_vblank;
        total_z80=z80_run(aim_z80);
	}
  }
#endif

  // 6 button pad: let's just say it timed out now
  Pico.m.padTHPhase[0]=Pico.m.padTHPhase[1]=0;

  // Active Scan:
  if (Pico.video.reg[1]&0x40) Pico.video.status&=~8; // Come out of vblank if display is enabled
  SekInterrupt(0); // Clear IRQ

  // Run in sections:
  for (aim=cycles_68k_vblank+cycles_68k_block, sects=16; sects; aim+=cycles_68k_block, sects--)
  {
	if (CheckIdle()) break;

    total+=SekRun(aim-total);
	if((PicoOpt&4) && Pico.m.z80Run) {
	  if(PicoOpt&1) {
#ifdef ARM9_SOUND
	    z80lines += 14; // 14 lines per section
	    for(; z80line < z80lines; z80line++) {
	      aim_z80+=cycles_z80_line;
          total_z80+=z80_run(aim_z80-total_z80);
	      sound_timers_and_dac(z80line);
	    }
#endif
      } else {
        aim_z80+=cycles_z80_block;
#ifdef ARM9_SOUND
        total_z80+=z80_run(aim_z80-total_z80);
#endif
      }
	}
  }

#ifdef ARM9_SOUND
  // todo: detect z80 idle too?
  if(sects && (PicoOpt&5) == 5 && Pico.m.z80Run) {
    z80lines += 14*sects;
    for(; z80line < z80lines; z80line++) {
      aim_z80+=cycles_z80_line;
      total_z80+=z80_run(aim_z80-total_z80);
      sound_timers_and_dac(z80line);
    }
  }
#endif

  // Dave was running 17 sections, but it looks like there should be only 16
  // it is probably some sort of overhead of this method,
  // because some games (Sensible Soccer, Second Samurai) no longer boot without this
  if(!sects) {
    aim+=cycles_68k_block;
	total+=SekRun(aim-total);
#ifdef ARM9_SOUND
    if((PicoOpt&4) && Pico.m.z80Run) {
      aim_z80+=cycles_z80_block;
      total_z80+=z80_run(aim_z80-total_z80);
    }
#endif
  }
  // EndProfile("Emulation");

  // BeginProfile();
  if(!PicoSkipFrame) {
    if(!(PicoOpt&0x10))
      // Draw the screen
      for (y=0;y<224;y++) PicoLine(y);
    else PicoFrameFull();
  }
  // EndProfile("PicoFrameFull");

  return 0;
}

int PicoFrame()
{
  int hints;

  //if (Pico.rom==NULL) return 1; // No Rom plugged in

  //PmovUpdate();

  hints=Pico.video.reg[0]&0x10;
  // don't use hints in alternative render mode, as hint effects will not be rendered anyway
  if(PicoOpt&0x10) hints = 0;

  //if(Pico.video.reg[12]&0x2) Pico.video.status ^= 0x10; // change odd bit in interlace mode

  // clear sound buffer
#ifdef ARM9_SOUND
  if(PsndOut) memset(PsndOut, 0, (PicoOpt & 8) ? (PsndLen<<2) : (PsndLen<<1));
#endif

  if(hints)
       PicoFrameHints();
  else PicoFrameSimple();

#ifdef ARM9_SOUND
  if(PsndOut) sound_render();
#endif

  return 0;
}

static int DefaultCram(int cram)
{
  int high=0x0841;
  // Convert 0000bbbb ggggrrrr
  // to      rrrr1ggg g10bbbb1
  high|=(cram&0x00f)<<12; // Red
  high|=(cram&0x0f0)<< 3; // Green
  high|=(cram&0xf00)>> 7; // Blue
  return high;
}

// Cram directly to BMP16 BG values for writing to an exrot BG
static int BGCram(int cram)
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
// int (*PicoCram)(int cram)=DefaultCram;
int (*PicoCram)(int cram)=BGCram;
