// This is part of Pico Library

// Most code (c) Copyright 2004 Dave, All rights reserved.
// Some code (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include <string.h>
#include "sound.h"
#include "ym2612.h"
#include "sn76496.h"

#include "driver.h"
#if defined(_USE_MZ80)
#include "mz80.h"
#elif defined(_USE_DRZ80)
#include "DrZ80.h"
#endif

#include "../PicoInt.h"

#ifdef ARM9_SOUND

int rasters_total; // total num of rasters per frame

// dac
short *dac_out;
void (*dac_next)(int raster) = 0;

// for Pico
int PsndRate=0;
int PsndLen=0; // number of mono samples, multiply by 2 for stereo
short *PsndOut=NULL; // PCM data buffer

// from ym2612.c
extern int   *ym2612_dacen;
extern INT32 *ym2612_dacout;
void YM2612TimerHandler(int c,int cnt);


// dac handlers, somewhat nasty
void dac_next_shrink(int raster)
{
  static int dac_cnt;

  if(!raster) {
    dac_cnt = rasters_total - PsndLen;
    dac_out = PsndOut;
    if(*ym2612_dacen)
      *dac_out = (short) *ym2612_dacout; // take a sample left from prev frame
	dac_out++;
  } else {
    // shrinking algo
    if(dac_cnt < 0) {
      // advance pointer
      if(*ym2612_dacen)
        *dac_out = (short) *ym2612_dacout;
      dac_out++;
      dac_cnt += rasters_total;
    }
    dac_cnt -= PsndLen;
  }
}

// fixme: stereo versions should also regard panning..
void dac_next_shrink_stereo(int raster)
{
  static int dac_cnt;

  if(!raster) {
    dac_cnt = rasters_total - PsndLen;
    dac_out = PsndOut;
    if(*ym2612_dacen)
      *dac_out = *(dac_out+1) = (short) *ym2612_dacout;
    dac_out+=2;
  } else {
    if(dac_cnt < 0) {
      if(*ym2612_dacen)
        *dac_out = *(dac_out+1) = (short) *ym2612_dacout;
      dac_out+=2;
      dac_cnt += rasters_total;
    }
    dac_cnt -= PsndLen;
  }
}

void dac_next_stretch(int raster)
{
  static int dac_cnt;

  if(!raster) {
    dac_cnt = PsndLen;
    dac_out = PsndOut;
  }

  if(raster == rasters_total-1) {
    if(*ym2612_dacen)
      for(; dac_out < PsndOut+PsndLen; dac_out++)
	    *dac_out = (short) *ym2612_dacout;
  } else {
    while(dac_cnt >= 0) {
      dac_cnt -= rasters_total;
      if(*ym2612_dacen)
        *dac_out = (short) *ym2612_dacout;
      dac_out++;
    }
  }
  dac_cnt += PsndLen;
}

void dac_next_stretch_stereo(int raster)
{
  static int dac_cnt;

  if(!raster) {
    dac_cnt = PsndLen;
    dac_out = PsndOut;
  }

  if(raster == rasters_total-1) {
    if(*ym2612_dacen)
      for(; dac_out < PsndOut+(PsndLen<<1); dac_out+=2)
	    *dac_out = *(dac_out+1) = (short) *ym2612_dacout;
  } else {
    while(dac_cnt >= 0) {
      dac_cnt -= rasters_total;
      if(*ym2612_dacen)
        *dac_out = *(dac_out+1) = (short) *ym2612_dacout;
      dac_out+=2;
    }
  }
  dac_cnt += PsndLen;
}


// also re-inits masked sound chips, because PicoOpt or Pico.m.pal might have changed
void sound_reset(int mask)
{
  if(mask & 1) {
    YM2612Init(Pico.m.pal ? OSC_PAL/7 : OSC_NTSC/7, PsndRate);
    YM2612ResetChip();
  }

  if(mask & 2)
    SN76496_init(Pico.m.pal ? OSC_PAL/15 : OSC_NTSC/15, PsndRate);

  // calculate PsndLen
  PsndLen=PsndRate/(Pico.m.pal ? 50 : 60);

  // for dac data outputing
  rasters_total = Pico.m.pal ? 312 : 262;
  if(PsndLen <= rasters_total)
       dac_next = (PicoOpt & 8) ? dac_next_shrink_stereo  : dac_next_shrink;
  else dac_next = (PicoOpt & 8) ? dac_next_stretch_stereo : dac_next_stretch;
}


// This is called once per raster (aka line)
int sound_timers_and_dac(int raster)
{
  if(PsndOut)
    dac_next(raster);

  // Our raster lasts 63.61323/64.102564 microseconds (NTSC/PAL)
  YM2612PicoTick();

  return 0;
}


int sound_render()
{
  // PSG
  if(PicoOpt & 2)
    SN76496Update(PsndOut, PsndLen, PicoOpt & 8);

  // Add in the stereo FM buffer
  if(PicoOpt & 1)
    YM2612UpdateOne(PsndOut, PsndLen, PicoOpt & 8);


  // dave's FM
  //PsndRender();

  return 0;
}



#if defined(_USE_MZ80)

// memhandlers for mz80 core
unsigned char mz80_read(UINT32 a,  struct MemoryReadByte *w)  { return z80_read(a); }
void mz80_write(UINT32 a, UINT8 d, struct MemoryWriteByte *w) { z80_write(d, a); }

// structures for mz80 core
static struct MemoryReadByte mz80_mem_read[]=
{
  {0x2000,0xffff,mz80_read},
  {(UINT32) -1,(UINT32) -1,NULL}
};
static struct MemoryWriteByte mz80_mem_write[]=
{
  {0x2000,0xffff,mz80_write},
  {(UINT32) -1,(UINT32) -1,NULL}
};
static struct z80PortRead mz80_io_read[] ={
  {(UINT16) -1,(UINT16) -1,NULL}
};
static struct z80PortWrite mz80_io_write[]={
  {(UINT16) -1,(UINT16) -1,NULL}
};

#elif defined(_USE_DRZ80)

static struct DrZ80 drZ80;

unsigned int DrZ80_rebasePC(unsigned short a)
{
  // don't know if this makes sense
  drZ80.Z80PC_BASE = (unsigned int) Pico.zram;// - (a&0xe000);
  return drZ80.Z80PC_BASE + a;
}

unsigned int DrZ80_rebaseSP(unsigned short a)
{
  drZ80.Z80SP_BASE = (unsigned int) Pico.zram;// - (a&0xe000);
  return drZ80.Z80SP_BASE + a;
  //return a;
}

unsigned char DrZ80_in(unsigned char p)
{
  return 0xff;
}

void DrZ80_out(unsigned char p,unsigned char d)
{
}

void DrZ80_irq_callback()
{
  drZ80.Z80_IRQ = 0; // lower irq when accepted
}

#endif

// z80 functionality wrappers
void z80_init()
{
#if defined(_USE_MZ80)
  struct mz80context z80;

  // z80
  mz80init();
  // Modify the default context
  mz80GetContext(&z80);
  
  // point mz80 stuff
  z80.z80Base=Pico.zram;
  z80.z80MemRead=mz80_mem_read;
  z80.z80MemWrite=mz80_mem_write;
  z80.z80IoRead=mz80_io_read;
  z80.z80IoWrite=mz80_io_write;
  
  mz80SetContext(&z80);

#elif defined(_USE_DRZ80)

  memset(&drZ80, 0, sizeof(struct DrZ80));
  drZ80.z80_rebasePC=DrZ80_rebasePC;
  drZ80.z80_rebaseSP=DrZ80_rebaseSP;
  drZ80.z80_read8   =z80_read;
  drZ80.z80_read16  =z80_read16;
  drZ80.z80_write8  =z80_write;
  drZ80.z80_write16 =z80_write16;
  drZ80.z80_in      =DrZ80_in;
  drZ80.z80_out     =DrZ80_out;
  drZ80.z80_irq_callback=DrZ80_irq_callback;
#endif
}

void z80_reset()
{
#if defined(_USE_MZ80)
  mz80reset();
#elif defined(_USE_DRZ80)
  memset(&drZ80, 0, 0x4c);
  drZ80.Z80F  = (1<<2);  // set ZFlag
  drZ80.Z80F2 = (1<<2);  // set ZFlag
  drZ80.Z80IX = 0xFFFF << 16;
  drZ80.Z80IY = 0xFFFF << 16;
  drZ80.Z80IM = 0; // 1?
  drZ80.Z80PC = drZ80.z80_rebasePC(0);
  drZ80.Z80SP = drZ80.z80_rebaseSP(0x2000); // 0xf000 ?
#endif
  Pico.m.z80_fakeval = 0; // for faking when Z80 is disabled
}

void z80_resetCycles()
{
#if defined(_USE_MZ80)
  mz80GetElapsedTicks(1);
#endif
}

void z80_int()
{
#if defined(_USE_MZ80)
  mz80int(0);
#elif defined(_USE_DRZ80)
  drZ80.z80irqvector = 0xFF; // default IRQ vector RST opcode
  drZ80.Z80_IRQ = 1;
#endif
}

// returns number of cycles actually executed
int z80_run(int cycles)
{
#if defined(_USE_MZ80)
  int ticks_pre = mz80GetElapsedTicks(0);
  mz80exec(cycles);
  return mz80GetElapsedTicks(0) - ticks_pre;
#elif defined(_USE_DRZ80)
  return cycles - DrZ80Run(&drZ80, cycles);
#else
  return cycles;
#endif
}

void z80_pack(unsigned char *data)
{
#if defined(_USE_MZ80)
  struct mz80context mz80;
  *(int *)data = 0x00005A6D; // "mZ"
  mz80GetContext(&mz80);
  memcpy(data+4, &mz80.z80clockticks, sizeof(mz80)-5*4); // don't save base&memhandlers
#elif defined(_USE_DRZ80)
  *(int *)data = 0x005A7244; // "DrZ"
  drZ80.Z80PC = drZ80.z80_rebasePC(drZ80.Z80PC-drZ80.Z80PC_BASE);
  drZ80.Z80SP = drZ80.z80_rebaseSP(drZ80.Z80SP-drZ80.Z80SP_BASE);
  memcpy(data+4, &drZ80, 0x4c);
#endif
}

void z80_unpack(unsigned char *data)
{
#if defined(_USE_MZ80)
  if(*(int *)data == 0x00005A6D) { // "mZ" save?
    struct mz80context mz80;
    mz80GetContext(&mz80);
    memcpy(&mz80.z80clockticks, data+4, sizeof(mz80)-5*4);
    mz80SetContext(&mz80);
  } else {
    z80_reset();
	z80_int();
  }
#elif defined(_USE_DRZ80)
  if(*(int *)data == 0x005A7244) { // "DrZ" save?
    memcpy(&drZ80, data+4, 0x4c);
    // update bases
    drZ80.Z80PC = drZ80.z80_rebasePC(drZ80.Z80PC-drZ80.Z80PC_BASE);
    drZ80.Z80SP = drZ80.z80_rebaseSP(drZ80.Z80SP-drZ80.Z80SP_BASE);
  } else {
    z80_reset();
	drZ80.Z80IM = 1;
	z80_int(); // try to goto int handler, maybe we won't execute trash there?
  }
#endif
}

void z80_exit()
{
#if defined(_USE_MZ80)
  mz80shutdown();
#endif
}

#endif // ARM9_SOUND
