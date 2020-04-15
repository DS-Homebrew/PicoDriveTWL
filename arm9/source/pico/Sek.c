// This is part of Pico Library

// Most code (c) Copyright 2004 Dave, All rights reserved.
// Some code (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "PicoInt.h"


#ifdef EMU_C68K
// ---------------------- Cyclone 68000 ----------------------

struct Cyclone PicoCpu;

int SekInit()
{
  CycloneInit();
  memset(&PicoCpu,0,sizeof(PicoCpu));
  return 0;
}

// Reset the 68000:
int SekReset()
{
  if (Pico.rom==NULL) return 1;

  CycloneReset(&PicoCpu);

  return 0;
}


// Run the 68000 for 'cyc' number of cycles and return the number of cycles actually executed
static inline int DoRun(int cyc)
{
	PicoCpu.cycles=cyc;
	CycloneRun(&PicoCpu);
	return cyc-PicoCpu.cycles;
}

int SekInterrupt(int irq)
{
  PicoCpu.irq=irq; // cancel
  return 0;
}

int SekPc() { return PicoCpu.pc-PicoCpu.membase; }

void SekState(unsigned char *data)
{
  memcpy(data,PicoCpu.d,0x44);
}

#endif

#ifdef EMU_A68K
// ---------------------- A68K ----------------------

void __cdecl M68000_RUN();
void __cdecl M68000_RESET();
int m68k_ICount=0;
unsigned int mem_amask=0xffffff; // 24-bit bus
unsigned int mame_debug=0,cur_mrhard=0,m68k_illegal_opcode=0,illegal_op=0,illegal_pc=0,opcode_entry=0; // filler

static int IrqCallback(int i) { i; return -1; }
static int DoReset() { return 0; }
static int (*ResetCallback)()=DoReset;

#pragma warning (disable:4152)

int SekInit()
{
  memset(&M68000_regs,0,sizeof(M68000_regs));
  M68000_regs.IrqCallback=IrqCallback;
  M68000_regs.pResetCallback=ResetCallback;
  M68000_RESET(); // Init cpu emulator
  return 0;
}

int SekReset()
{
  // Reset CPU: fetch SP and PC
  M68000_regs.srh=0x27; // Supervisor mode
  M68000_regs.a[7]=PicoRead32(0);
  M68000_regs.pc  =PicoRead32(4);
  PicoInitPc(M68000_regs.pc);

  return 0;
}

static __inline int DoRun(int cyc)
{
  m68k_ICount=cyc;
  M68000_RUN();
  return cyc-m68k_ICount;
}

int SekInterrupt(int irq)
{
  M68000_regs.irq=irq; // raise irq (gets lowered after taken)
  return 0;
}

int SekPc() { return M68000_regs.pc; }

void SekState(unsigned char *data)
{
  memcpy(data,      M68000_regs.d, 0x40);
  memcpy(data+0x40,&M68000_regs.pc,0x04);
}

#endif

#ifdef EMU_NULL
// -----------------------------------------------------------
int SekInit() { return 0; }
int SekReset() { return 0; }
static inline int DoRun(int cyc) { return cyc; }
int SekInterrupt(int) { return 0; }

int SekPc()
{
  return 0;
}

void SekState(unsigned char *) { }

#endif

int SekRun(int cyc)
{
  int did=0;

  did=DoRun(cyc);

  return did;
}
