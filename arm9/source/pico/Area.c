// This is part of Pico Library

// Most code (c) Copyright 2004 Dave, All rights reserved.
// Some code (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "PicoInt.h"

// ym2612
#include "sound/ym2612.h"
extern void *ym2612_regs;

// sn76496
#ifdef ARM9_SOUND
extern int *sn76496_regs;
#endif

int (*PicoAcb)(struct PicoArea *)=NULL; // Area callback for each block of memory
void *PmovFile=NULL;
int PmovAction=0;
arearw *areaRead  = (arearw *) fread;  // read and write function pointers for
arearw *areaWrite = (arearw *) fwrite; // gzip save state ability


// Scan one variable and callback
static int ScanVar(void *data,int len,char *name)
{
  struct PicoArea pa={NULL,0,NULL};
  if (PicoAcb) {
    pa.data=data; pa.len=len; pa.name=name;
    return PicoAcb(&pa);
  }
  return 1;
}

#define SCAN_VAR(x,y) ScanVar(&x,sizeof(x),y);
#define SCANP(x)      ScanVar(&Pico.x,sizeof(Pico.x),#x);

// Pack the cpu into a common format:
static int PackCpu(unsigned char *cpu)
{
  unsigned int pc=0;

#ifdef EMU_A68K
  memcpy(cpu,M68000_regs.d,0x40);
  pc=M68000_regs.pc;
  *(unsigned char *)(cpu+0x44)=(unsigned char)M68000_regs.ccr;
  *(unsigned char *)(cpu+0x45)=(unsigned char)M68000_regs.srh;
  *(unsigned int  *)(cpu+0x48)=M68000_regs.isp;
#endif

#ifdef EMU_C68K
  memcpy(cpu,PicoCpu.d,0x40);
  pc=PicoCpu.pc-PicoCpu.membase;
  *(unsigned char *)(cpu+0x44)=PicoCpu.flags;
  *(unsigned char *)(cpu+0x45)=PicoCpu.srh;
  *(unsigned int  *)(cpu+0x48)=PicoCpu.osp;
#endif

  *(unsigned int *)(cpu+0x40)=pc;
  return 0;
}

static int UnpackCpu(unsigned char *cpu)
{
  unsigned int pc=0;

  pc=*(unsigned int *)(cpu+0x40);

#ifdef EMU_A68K
  memcpy(M68000_regs.d,cpu,0x40);
  M68000_regs.pc=pc;
  M68000_regs.ccr=*(unsigned char *)(cpu+0x44);
  M68000_regs.srh=*(unsigned char *)(cpu+0x45);
  M68000_regs.isp=*(unsigned int  *)(cpu+0x48);
#endif

#ifdef EMU_C68K
  memcpy(PicoCpu.d,cpu,0x40);
  PicoCpu.membase=0;
  PicoCpu.pc =PicoCpu.checkpc(pc); // Base pc
  PicoCpu.flags=*(unsigned char *)(cpu+0x44);
  PicoCpu.srh=*(unsigned char *)(cpu+0x45);
  PicoCpu.osp=*(unsigned int  *)(cpu+0x48);
#endif
  return 0;
}

// Scan the contents of the virtual machine's memory for saving or loading
static int PicoAreaScan(int action,unsigned int ver)
{
  unsigned char cpu[0x60];
  unsigned char cpu_z80[0x60];
  int ret;

  memset(&cpu,0,sizeof(cpu));
  memset(&cpu_z80,0,sizeof(cpu_z80));

  if (action&4)
  {
    Pico.m.scanline=0;

	// Scan all the memory areas:
    SCANP(ram) SCANP(vram) SCANP(zram) SCANP(cram) SCANP(vsram)

    // Pack, scan and unpack the cpu data:
    if((PmovAction&3)==1) PackCpu(cpu);
    //SekInit();     // notaz: do we really have to do this here?
    //PicoMemInit();
    SCAN_VAR(cpu,"cpu")
    if((PmovAction&3)==2) UnpackCpu(cpu);

    SCAN_VAR(Pico.m    ,"misc")
    SCAN_VAR(Pico.video,"video")


	if(ver == 0x0030) { // zram was being saved incorrectly in 0x0030 (byteswaped?)
	  Byteswap(Pico.zram, 0x2000);
	  return 0; // do not try to load sound stuff
    }

	//SCAN_VAR(Pico.s    ,"sound")
	// notaz: save/load z80, YM2612, sn76496 states instead of Pico.s (which is unused anyway)
	if(PicoOpt&7) {
#ifdef ARM9_SOUND
	  if((PmovAction&3)==1) z80_pack(cpu_z80);
      ret = SCAN_VAR(cpu_z80,"cpu_z80")
	  // do not unpack if we fail to load z80 state
	  if((PmovAction&3)==2) {
        if(ret) z80_reset();
        else    z80_unpack(cpu_z80);
      }
#endif
	}
	if(PicoOpt&3)
#ifdef ARM9_SOUND
      ScanVar(sn76496_regs,28*4,"SN76496state"); // regs and other stuff
#endif
	if(PicoOpt&1) {
#ifdef ARM9_SOUND
      ScanVar(ym2612_regs, 0x200+4, "YM2612state"); // regs + addr line
	  if((PmovAction&3)==2) YM2612PicoStateLoad(); // reload YM2612 state from it's regs
#endif
	}
  }

  return 0;
}

// ---------------------------------------------------------------------------
// Helper code to save/load to a file handle

// Area callback for each piece of Megadrive memory, read or write to the file:
static int StateAcb(struct PicoArea *pa)
{
  int ret=0;
  if (PmovFile==NULL) return 1;

  if ((PmovAction&3)==1) ret = areaWrite(pa->data,1,pa->len,PmovFile);
  if ((PmovAction&3)==2) ret = areaRead (pa->data,1,pa->len,PmovFile);
  return (ret != pa->len);
}

// Save or load the state from PmovFile:
int PmovState()
{
  int minimum=0;
  unsigned char head[32];

  memset(head,0,sizeof(head));

  // Find out minimal compatible version:
  //PicoAreaScan(PmovAction&0xc,&minimum);
  minimum = 0x0021;

  memcpy(head,"Pico",4);
  *(unsigned int *)(head+0x8)=PicoVer;
  *(unsigned int *)(head+0xc)=minimum;

  // Scan header:
  if (PmovAction&1) areaWrite(head,1,sizeof(head),PmovFile);
  if (PmovAction&2) areaRead (head,1,sizeof(head),PmovFile);

  // Scan memory areas:
  PicoAcb=StateAcb;
  PicoAreaScan(PmovAction,*(unsigned int *)(head+0x8));
  PicoAcb=NULL;
  return 0;
}

int PmovUpdate()
{
  int ret=0;
  if (PmovFile==NULL) return 1;
  
  if ((PmovAction&3)==0) return 0;

  PicoPad[1]=0; // Make sure pad #2 is blank
  if (PmovAction&1) ret=areaWrite(PicoPad,1,2,PmovFile);
  if (PmovAction&2) ret=areaRead (PicoPad,1,2,PmovFile);

  if (ret!=2)
  {
    // End of file
    fclose((FILE *) PmovFile); PmovFile=NULL;
    PmovAction=0;
  }

  return 0;
}
