// This is part of Pico Library

// Most code (c) Copyright 2004 Dave, All rights reserved.
// Some code (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "PicoInt.h"
char* romSpace;

void Byteswap(unsigned char *data,int len)
{
  int i=0;

  if (len<2) return; // Too short

  do
  {
    unsigned short *pd=(unsigned short *)(data+i);
    int value=*pd; // Get 2 bytes

    value=(value<<8)|(value>>8); // Byteswap it
    *pd=(unsigned short)value; // Put 2b ytes
    i+=2;
  }  
  while (i+2<=len);
}

// Interleve a 16k block and byteswap
static int InterleveBlock(unsigned char *dest,unsigned char *src)
{
  int i=0;
  for (i=0;i<0x2000;i++) dest[(i<<1)  ]=src[       i]; // Odd
  for (i=0;i<0x2000;i++) dest[(i<<1)+1]=src[0x2000+i]; // Even
  return 0;
}

// Decode a SMD file
int DecodeSmd(unsigned char *data,int len)
{
  unsigned char *temp=NULL;
  int i=0;

  temp=(unsigned char *)malloc(0x4000);
  if (temp==NULL) return 1;
  memset(temp,0,0x4000);

  // Interleve each 16k block and shift down by 0x200:
  for (i=0; i+0x4200<=len; i+=0x4000)
  {
    InterleveBlock(temp,data+0x200+i); // Interleve 16k to temporary buffer
    memcpy(data+i,temp,0x4000); // Copy back in
  }

  free(temp);
  return 0;
}

int PicoCartLoad(FILE *f,unsigned char **prom,unsigned int *psize)
{
  int size=0;
  if (f==NULL) return 1;

  fseek(f,0,SEEK_END); size=ftell(f); fseek(f,0,SEEK_SET);

  size=(size+3)&~3; // Round up to a multiple of 4

  fread(romSpace,1,size,f); // Load up the rom
  fclose(f);

  // Check for SMD:
  if ((size&0x3fff)==0x200) { DecodeSmd((unsigned char*)romSpace,size); size-=0x200; } // Decode and byteswap SMD
  else Byteswap((unsigned char*)romSpace,size); // Just byteswap
  
  if (prom)  *prom=(unsigned char*)romSpace;
  if (psize) *psize=size;

  return 0;
}

// Insert/remove a cartridge:
int PicoCartInsert(unsigned char *rom,unsigned int romsize)
{
  // Make sure movie playing/recording is stopped:
  if (PmovFile) fclose(PmovFile);
  PmovFile=NULL; PmovAction=0;

  // notaz: add a 68k "jump one op back" opcode to the end of ROM.
  // This will hang the emu, but will prevent nasty crashes.
  // note: 4 bytes are padded to every ROM
  *(unsigned long *)(rom+romsize) = 0xFEFFFA4E;

  // notaz: sram
  SRam.resize=1;
  Pico.rom=rom;
  Pico.romsize=romsize;

  return PicoReset(1);
}


#ifdef __SYMBIAN32__

// notaz
#include "../unzip.h"

// nearly same as PicoCartLoad, but works with zipfiles
int CartLoadZip(const char *fname, unsigned char **prom, unsigned int *psize)
{
	unsigned char *rom=0;
	struct zipent* zipentry;
	int size;
	ZIP *zipfile = openzip(fname);

	if(!zipfile) return 1;

	// find first bin or smd
	while((zipentry = readzip(zipfile)) != 0)
	{
		char *ext;
		if(strlen(zipentry->name) < 5) continue;
		ext = zipentry->name+strlen(zipentry->name)-4;
		
		if(!strcasecmp(ext, ".bin") || !strcasecmp(ext, ".smd") || !strcasecmp(ext, ".gen")) break;
	}

	if(!zipentry) {
		closezip(zipfile);
		return 4; // no roms
	}

	size = zipentry->uncompressed_size;

	size=(size+3)&~3; // Round up to a multiple of 4

	// Allocate space for the rom plus padding
	rom=(unsigned char *)malloc(size+4);
	if (rom==NULL) { closezip(zipfile); return 1; }
	//memset(rom,0,size+4);

	if(readuncompresszip(zipfile, zipentry, (char *)rom) != 0) {
		free(rom);
		rom = 0;
		closezip(zipfile);
		return 5; // unzip failed
	}

	closezip(zipfile);

	// Check for SMD:
	if ((size&0x3fff)==0x200) { DecodeSmd(rom,size); size-=0x200; } // Decode and byteswap SMD
	else Byteswap(rom,size); // Just byteswap

	if (prom)  *prom=rom;
	if (psize) *psize=size;

	return 0;
}

#endif
