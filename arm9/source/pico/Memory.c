// This is part of Pico Library

// Most code (c) Copyright 2004 Dave, All rights reserved.
// Some code (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


//#define __debug_io

#include <maxmod9.h>
#include "PicoInt.h"

#include "sound/ym2612.h"
#include "sound/sn76496.h"

// typedef unsigned char  u8;
// typedef unsigned short u16;
// typedef unsigned int   u32;


static __inline int PicoMemBase(u32 pc)
{
  int membase=0;

  if (pc<Pico.romsize)
  {
    membase=(int)Pico.rom; // Program Counter in Rom
  }
  else if ((pc&0xe00000)==0xe00000)
  {
    membase=(int)Pico.ram-(pc&0xff0000); // Program Counter in Ram
  }
  else
  {
    // Error - Program Counter is invalid
    membase=(int)Pico.rom;
  }

  return membase;
}


#ifdef EMU_A68K
extern u8 *OP_ROM=NULL,*OP_RAM=NULL;

static void CPU_CALL PicoCheckPc(u32 pc)
{
  OP_ROM=(u8 *)PicoMemBase(pc);

  // don't bother calling us back unless it's outside the 64k segment
  M68000_regs.AsmBank=(pc>>16);
}
#endif

#ifdef EMU_C68K
static u32 PicoCheckPc(u32 pc)
{
  //u32 res, add;
  //u16 op;
  pc-=PicoCpu.membase; // Get real pc
  pc&=0xffffff;

  PicoCpu.membase=PicoMemBase(pc);

  return PicoCpu.membase+pc;
/* ineffective
  // check for common situation with tst and branch opcode waiting loops
  // cyclone must be prepared for this to work
  if(((op=*(u16 *)res) & 0xFF00) == 0x4A00) { // we are jumping on tst opcode
    add = 2;
    if((op & 0x38) == 0x38) add += 2 << (op&1); // with word or long operand
	op = *(u16 *)(res+add); add += 2;
	if((op >> 12) == 6 && (char)op == (char)-add) // next op is branch back to tst
	  PicoCpu.cycles=8; // burn cycles
  }
  return res;
*/
}
#endif

#ifdef EMU_NULL
static u32 PicoCheckPc(u32) { return 0; }
#endif


int PicoInitPc(u32 pc)
{
  PicoCheckPc(pc);
  return 0;
}

// -----------------------------------------------------------------
static int PadRead(int i)
{
  int pad=0,value=0,TH;
  pad=~PicoPad[i]; // Get inverse of pad MXYZ SACB RLDU
  TH=Pico.ioports[i+1]&0x40;

  if(PicoOpt & 0x20) { // 6 button gamepad enabled
    int phase = Pico.m.padTHPhase[i];

	if(phase == 2 && !TH) {
	  value=(pad&0xc0)>>2;              // ?0SA 0000
	  goto end;
    } else if(phase == 3 && TH) {
	  value=(pad&0x30)|((pad>>8)&0xf);  // ?1CB MXYZ
	  goto end;
    } else if(phase == 3 && !TH) {
	  value=((pad&0xc0)>>2)|0x0f;       // ?0SA 1111
	  goto end;
    }
  }

  if(TH) value=(pad&0x3f);              // ?1CB RLDU
  else   value=((pad&0xc0)>>2)|(pad&3); // ?0SA 00DU

  end:

  // orr the bits, which are set as output
  value |= Pico.ioports[i+1]&Pico.ioports[i+4];

  return value; // will mirror later
}

extern bool playSound;
extern mm_sfxhand sndHandlers[48];
static int prevSndId = 0;

extern u8 sndFirstID;
extern u8 sndLastID;

extern u16 snd68000addr[2];
extern u16 sndZ80addr[2];

static void SoundPlayRAM(void) {
	if (!playSound || snd68000addr[0]==0) return;

	int soundId = 0;
	if (Pico.ram[snd68000addr[0]] >= sndFirstID && Pico.ram[snd68000addr[0]] <= sndLastID) {
		soundId = Pico.ram[snd68000addr[0]];
	} else if (Pico.ram[snd68000addr[1]] >= sndFirstID && Pico.ram[snd68000addr[1]] <= sndLastID) {
		soundId = Pico.ram[snd68000addr[1]];
	}
	if (soundId==0) return;

	soundId -= sndFirstID;

	// External sound
	if (sndHandlers[prevSndId] != NULL)
		mmEffectRelease( sndHandlers[prevSndId] );
	sndHandlers[soundId] = mmEffect(soundId);
	
	prevSndId = soundId;
}

static void SoundPlayZ80(void) {
	if (!playSound || sndZ80addr[0]==0) return;

	int soundId = 0;
	if (Pico.zram[sndZ80addr[0]] >= sndFirstID && Pico.zram[sndZ80addr[0]] <= sndLastID) {
		soundId = Pico.zram[sndZ80addr[0]];
	} else if (Pico.zram[sndZ80addr[1]] >= sndFirstID && Pico.zram[sndZ80addr[1]] <= sndLastID) {
		soundId = Pico.zram[sndZ80addr[1]];
	}
	if (soundId==0) return;

	soundId -= sndFirstID;

	// External sound
	if (sndHandlers[prevSndId] != NULL)
		mmEffectRelease( sndHandlers[prevSndId] );
	sndHandlers[soundId] = mmEffect(soundId);
	
	prevSndId = soundId;
}

// notaz: address must already be checked
static int SRAMRead(u32 a)
{
  return *(u16 *)(SRam.data-SRam.start+a);
}

static u32 OtherRead16(u32 a)
{
  u32 d=0;

  if ((a&0xffc000)==0xa00000)
  {
	a&=0x1fff;

	if(!(PicoOpt&4)) {
      // Z80 disabled, do some faking
	  static int zerosent = 0;
	  if(a == Pico.m.z80_lastaddr) { // probably polling something
        d = Pico.m.z80_fakeval;
		if((d & 0xf) == 0xf && !zerosent) {
		  d = 0; zerosent = 1;
        } else {
		  Pico.m.z80_fakeval++;
		  zerosent = 0;
		}
        goto end;
	  } else {
        Pico.m.z80_fakeval = 0;
	  }

	  Pico.m.z80_lastaddr = (u16) a;
	}

    // Z80 ram (not byteswaped)
    d=(Pico.zram[a]<<8)|Pico.zram[a+1];

    goto end;
  }

  if ((a&0xfffffc)==0xa04000) { 
#ifdef ARM9_SOUND
	  if(PicoOpt&1) 
		  d=YM2612Read(a); 
	  else 
#endif
		  d=Pico.m.rotate++&3; 
	  goto end; } // Fudge if disabled
  if ((a&0xffffe0)==0xa10000) { // I/O ports
    a=(a>>1)&0xf;
    switch(a) {
	  case 0:  d=Pico.m.hardware; break; // Hardware value (Version register)
	  case 1:  d=PadRead(0); d|=Pico.ioports[1]&0x80; break;
	  case 2:  d=PadRead(1); d|=Pico.ioports[2]&0x80; break;
	  default: d=Pico.ioports[a]; break; // IO ports can be used as RAM
	}
	d|=d<<8;
    goto end;
  }
  if (a==0xa11100) { d=Pico.m.z80Run<<8; goto end; }

  if ((a&0xffffe0)==0xc00000) { d=PicoVideoRead(a); goto end; }

end:
  return d;
}

static void OtherWrite8(u32 a,u32 d)
{
	extern void loadRomBank(int page, int i);

  if (a==0xc00011||a==0xa07F11){ 
#ifdef ARM9_SOUND
	  if(PicoOpt&2) SN76496Write(d); 
#endif
	  return; } // PSG Sound
  if ((a&0xffffe0)==0xc00000)  { PicoVideoWrite(a,d|(d<<8)); return; } // Byte access gets mirrored

  if ((a&0xffc000)==0xa00000)  { // Z80 ram
	Pico.zram[a&0x1fff]=(u8)d;
	SoundPlayZ80();
	return;
  }
  if ((a&0xfffffc)==0xa04000)  { 
#ifdef ARM9_SOUND
	  if(PicoOpt&1) YM2612Write(a, d); 
#endif
	  return; } // FM Sound
  if ((a&0xffffe0)==0xa10000)  { // I/O ports
    a=(a>>1)&0xf;
	// 6 button gamepad: if TH went from 0 to 1, gamepad changes state
	if(PicoOpt&0x20) {
      if(a==1) {
	    Pico.m.padDelay[0] = 0;
	    if(!(Pico.ioports[1]&0x40) && (d&0x40)) Pico.m.padTHPhase[0]++;
	  }
	  //else if(a==2) {
	  //  Pico.m.padDelay[1] = 0;
	  //  if(!(Pico.ioports[2]&0x40) && (d&0x40)) Pico.m.padTHPhase[1]++;
	  //}
	}
	Pico.ioports[a]=(u8)d; // IO ports can be used as RAM
    return;
  }
  if (a==0xa11100) { Pico.m.z80Run=(u8)(d^1); return; }
  if (a==0xa11200) { 
#ifdef ARM9_SOUND
	  if(!d) 
		  z80_reset(); 
#endif
	  return; 
  }

  if (a==0xa06000) // BANK register
  {
    Pico.m.z80_bank68k>>=1;
    Pico.m.z80_bank68k|=(d&1)<<8;
    Pico.m.z80_bank68k&=0x1ff; // 9 bits and filled in the new top one
    return;
  }

  // notaz: sram
  if((Pico.m.sram_reg & 2) == 0 && a >= SRam.start && a <= SRam.end) {
	u8 *pm=(u8 *)(SRam.data-SRam.start+a);
	if(*pm != (u8)d) {
	  Pico.m.sram_changed = 1;
      *pm=(u8)d;
	}
	return;
  }
  // sram access register
  if(a == 0xA130F1) {
    Pico.m.sram_reg = (u8)d;
	return;
  }
  // SSF2 mapper (0x080000-0x0FFFFF)
  if(a == 0xA130F3) {
	if (Pico.m.romBank[0] != d) {
		loadRomBank(d, 0);
		Pico.m.romBank[0] = d;
	}
	return;
  }
  // SSF2 mapper (0x100000-0x17FFFF)
  if(a == 0xA130F5) {
	if (Pico.m.romBank[1] != d) {
		loadRomBank(d, 1);
		Pico.m.romBank[1] = d;
	}
	return;
  }
  // SSF2 mapper (0x180000-0x1FFFFF)
  if(a == 0xA130F7) {
	if (Pico.m.romBank[2] != d) {
		loadRomBank(d, 2);
		Pico.m.romBank[2] = d;
	}
	return;
  }
  // SSF2 mapper (0x200000-0x27FFFF)
  if(a == 0xA130F9) {
	if (Pico.m.romBank[3] != d) {
		loadRomBank(d, 3);
		Pico.m.romBank[3] = d;
	}
	return;
  }
  // SSF2 mapper (0x280000-0x2FFFFF)
  if(a == 0xA130FB) {
	if (Pico.m.romBank[4] != d) {
		loadRomBank(d, 4);
		Pico.m.romBank[4] = d;
	}
	return;
  }
  // SSF2 mapper (0x300000-0x37FFFF)
  if(a == 0xA130FD) {
	if (Pico.m.romBank[5] != d) {
		loadRomBank(d, 5);
		Pico.m.romBank[5] = d;
	}
	return;
  }
  // SSF2 mapper (0x380000-0x3FFFFF)
  if(a == 0xA130FF) {
	if (Pico.m.romBank[6] != d) {
		loadRomBank(d, 6);
		Pico.m.romBank[6] = d;
	}
	return;
  }
}

static void OtherWrite16(u32 a,u32 d)
{
  if ((a&0xffffe0)==0xc00000) { PicoVideoWrite(a,d); return; }

  if ((a&0xffffe0)==0xa10000)  { // I/O ports
    a=(a>>1)&0xf;
	// 6 button gamepad: if TH went from 0 to 1, gamepad changes state
	if(PicoOpt&0x20) {
      if(a==1) {
	    Pico.m.padDelay[0] = 0;
	    if(!(Pico.ioports[1]&0x40) && (d&0x40)) Pico.m.padTHPhase[0]++;
	  }
	  else if(a==2) {
	    Pico.m.padDelay[1] = 0;
	    if(!(Pico.ioports[2]&0x40) && (d&0x40)) Pico.m.padTHPhase[1]++;
	  }
	}
	Pico.ioports[a]=(u8)d; // IO ports can be used as RAM
    return;
  }

  OtherWrite8(a,  d>>8);
  OtherWrite8(a+1,d&0xff);
}

// -----------------------------------------------------------------
//                     Read Rom and read Ram

static u8 CPU_CALL PicoRead8(u32 a)
{
  u32 d=0;
  a&=0xffffff;

  if ((a&0xe00000)==0xe00000) { d = *(u8 *)(Pico.ram+((a^1)&0xffff)); goto end; } // Ram

  // notaz: sram
  if(a >= SRam.start && a <= SRam.end && (Pico.m.sram_reg & 1)) {
    d = *(u8 *)(SRam.data-SRam.start+a);
	goto end;
  }

  if (a<Pico.romsize) { d = *(u8 *)(Pico.rom+(a^1)); goto end; } // Rom

  d=OtherRead16(a&~1); if ((a&1)==0) d>>=8;

  end:
#ifdef __debug_io
  dprintf("r8 : %06x,   %02x", a, (u8)d);
#endif
  return (u8)d;
}

u16 CPU_CALL PicoRead16(u32 a)
{
  u16 d=0;
  a&=0xfffffe;

  if ((a&0xe00000)==0xe00000) { d=*(u16 *)(Pico.ram+(a&0xffff)); goto end; } // Ram

  // notaz: sram
  if(a >= SRam.start && a <= SRam.end && (Pico.m.sram_reg & 1)) {
    d = (u16) SRAMRead(a);
	goto end;
  }

  if (a<Pico.romsize) { d = *(u16 *)(Pico.rom+a); goto end; } // Rom

  d = (u16)OtherRead16(a);

  end:
#ifdef __debug_io
  dprintf("r16: %06x, %04x", a, d);
#endif
  return d;
}

u32 CPU_CALL PicoRead32(u32 a)
{
  u32 d=0;
  a&=0xfffffe;

  if ((a&0xe00000)==0xe00000) { u16 *pm=(u16 *)(Pico.ram+(a&0xffff)); d = (pm[0]<<16)|pm[1]; goto end; } // Ram

  // notaz: sram
  if((Pico.m.sram_reg & 1) && a >= SRam.start && a <= SRam.end) {
    d = (SRAMRead(a)<<16)|SRAMRead(a+2);
	goto end;
  }

  if (a<Pico.romsize) { u16 *pm=(u16 *)(Pico.rom+a); d = (pm[0]<<16)|pm[1]; goto end; } // Rom

  d = (OtherRead16(a)<<16)|OtherRead16(a+2);

  end:
#ifdef __debug_io
  dprintf("r32: %06x, %08x", a, d);
#endif
  return d;
}

// -----------------------------------------------------------------
//                            Write Ram

static void CPU_CALL PicoWrite8(u32 a,u8 d)
{
#ifdef __debug_io
  dprintf("w8 : %06x,   %02x", a, d);
#endif
  if ((a&0xe00000)==0xe00000) {
    u8 *pm=(u8 *)(Pico.ram+((a^1)&0xffff));
    pm[0]=d;
    SoundPlayRAM();
    return;
  } // Ram

  a&=0xffffff;
  OtherWrite8(a,d);
}

static void CPU_CALL PicoWrite16(u32 a,u16 d)
{
#ifdef __debug_io
  dprintf("w16: %06x, %04x", a, d);
#endif
  if ((a&0xe00000)==0xe00000) { *(u16 *)(Pico.ram+(a&0xfffe))=d; return; } // Ram

  a&=0xfffffe;
  OtherWrite16(a,d);
}

static void CPU_CALL PicoWrite32(u32 a,u32 d)
{
#ifdef __debug_io
  dprintf("w32: %06x, %08x", a, d);
#endif
  if ((a&0xe00000)==0xe00000)
  {
    // Ram:
    u16 *pm=(u16 *)(Pico.ram+(a&0xfffe));
    pm[0]=(u16)(d>>16); pm[1]=(u16)d;
    return;
  }

  a&=0xfffffe;
  OtherWrite16(a,  (u16)(d>>16));
  OtherWrite16(a+2,(u16)d);
}


// -----------------------------------------------------------------
int PicoMemInit()
{
#ifdef EMU_C68K
  // Setup memory callbacks:
  #ifdef SPECIALIZED_CYCLONE
  PicoCpu.pad1[0]=(unsigned int)&Pico;
  PicoCpu.pad1[1]=(unsigned int)&Pico.rom;
  //extern void dprintf(char *format, ...);
  //PicoCpu.tst1="cyclone: invalid opcode, pc: %08x";
  //PicoCpu.tst2=&dprintf;
  //PicoCpu.tst3=&exit;
  #endif
  PicoCpu.checkpc=PicoCheckPc;
  PicoCpu.fetch8 =PicoCpu.read8 =PicoRead8;
  PicoCpu.fetch16=PicoCpu.read16=PicoRead16;
  PicoCpu.fetch32=PicoCpu.read32=PicoRead32;
  PicoCpu.write8 =PicoWrite8;
  PicoCpu.write16=PicoWrite16;
  PicoCpu.write32=PicoWrite32;
#endif
  return 0;
}

#ifdef EMU_A68K
struct A68KInter
{
  u32 unknown;
  u8  (__fastcall *Read8) (u32 a);
  u16 (__fastcall *Read16)(u32 a);
  u32 (__fastcall *Read32)(u32 a);
  void (__fastcall *Write8)  (u32 a,u8 d);
  void (__fastcall *Write16) (u32 a,u16 d);
  void (__fastcall *Write32) (u32 a,u32 d);
  void (__fastcall *ChangePc)(u32 a);
  u8  (__fastcall *PcRel8) (u32 a);
  u16 (__fastcall *PcRel16)(u32 a);
  u32 (__fastcall *PcRel32)(u32 a);
  u16 (__fastcall *Dir16)(u32 a);
  u32 (__fastcall *Dir32)(u32 a);
};

struct A68KInter a68k_memory_intf=
{
  0,
  PicoRead8,
  PicoRead16,
  PicoRead32,
  PicoWrite8,
  PicoWrite16,
  PicoWrite32,
  PicoCheckPc,
  PicoRead8,
  PicoRead16,
  PicoRead32,
  PicoRead16, // unused
  PicoRead32, // unused
};
#endif


// -----------------------------------------------------------------
//                        z80 memhandlers

unsigned char z80_read(unsigned short a)
{
  u8 ret = 0;

  if ((a>>13)==2) // 0x4000-0x5fff (Charles MacDonald)
  {
#ifdef ARM9_SOUND
    if(PicoOpt&1) ret = (u8) YM2612Read(a&3);
#endif
	goto end;
  }

  if (a>=0x8000)
  {
    u32 addr68k;
    addr68k=Pico.m.z80_bank68k<<15;
    addr68k+=a&0x7fff;

    ret = (u8) PicoRead8(addr68k);
	goto end;
  }

  // should not be needed || dprintf("z80_read RAM");
  if (a<0x2000) { ret = (u8) Pico.zram[a]; goto end; }

end:
  return ret;
}

unsigned short z80_read16(unsigned short a)
{
  //dprintf("z80_read16");

  return (u16) ( (u16)z80_read(a) | ((u16)z80_read((u16)(a+1))<<8) );
}

void z80_write(unsigned char data, unsigned short a)
{
  if ((a>>13)==2) // 0x4000-0x5fff (Charles MacDonald)
  {
#ifdef ARM9_SOUND
    if(PicoOpt&1) YM2612Write(a, data);
#endif
    return;
  }

  if ((a&0xfff8)==0x7f10&&(a&1)) // 7f11 7f13 7f15 7f17
  {
#ifdef ARM9_SOUND
    if(PicoOpt&2) SN76496Write(data);
#endif
    return;
  }

  if ((a>>8)==0x60)
  {
    Pico.m.z80_bank68k>>=1;
    Pico.m.z80_bank68k|=(data&1)<<8;
    Pico.m.z80_bank68k&=0x1ff; // 9 bits and filled in the new top one
    return;
  }

  if (a>=0x8000)
  {
    u32 addr68k;
    addr68k=Pico.m.z80_bank68k<<15;
    addr68k+=a&0x7fff;
    PicoWrite8(addr68k, data);
    return;
  }

  // sould not be needed, drZ80 knows how to access RAM itself || dprintf("z80_write RAM @ %08x", lr);
  if (a<0x2000) { Pico.zram[a]=data; return; }
}

void z80_write16(unsigned short data, unsigned short a)
{
  //dprintf("z80_write16");

  z80_write((unsigned char) data,a);
  z80_write((unsigned char)(data>>8),(u16)(a+1));
}

