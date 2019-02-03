// This is part of Pico Library

// Most code (c) Copyright 2004 Dave, All rights reserved.
// Some code (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "PicoInt.h"
#ifndef __GNUC__
#pragma warning (disable:4706) // Disable assignment within conditional
#endif

int (*PicoScan)(unsigned int num,unsigned short *data)=NULL;

// Line colour indices - in the format 00ppcccc pp=palette, cccc=colour
//static
unsigned short HighCol[32+320+8]; // Gap for 32 column, and messy border on right
static int HighCacheA[41+1]; // caches for high layers
static int HighCacheB[41+1];
static int HighCacheS[80+1]; // and sprites
static int rendstatus; // &1: sprite masking mode 2
int Scanline=0; // Scanline


struct TileStrip
{
  int nametab; // Position in VRAM of name table (for this tile line)
  int line;    // Line number in pixels 0x000-0x3ff within the virtual tilemap 
  int hscroll; // Horizontal scroll value in pixels for the line
  int xmask;   // X-Mask (0x1f - 0x7f) for horizontal wraparound in the tilemap
  int *hc;     // cache for high tile codes and their positions
  int cells;   // cells (tiles) to draw (32 col mode doesn't need to update whole 320)
};

// stuff available in asm:
#ifdef _ASM_DRAW_C
void DrawStrip(struct TileStrip *ts);
void DrawWindow(int tstart, int tend, int prio);
void BackFill(int reg7);
void DrawSprite(unsigned int *sprite,int **hc);
void DrawTilesFromCache(int *hc);
void DrawSpritesFromCache(int *hc);
void DrawLayer(int plane, int *hcache, int maxcells);
#endif


static int TileNorm(unsigned short *pd,int addr,unsigned short *pal)
{
  unsigned int pack=0; unsigned int t=0;

  pack=*(unsigned int *)(Pico.vram+addr); // Get 8 pixels
  if (pack)
  {
    t=pack&0x0000f000; if (t) { t=pal[t>>12]; pd[0]=(unsigned short)t; }
    t=pack&0x00000f00; if (t) { t=pal[t>> 8]; pd[1]=(unsigned short)t; }
    t=pack&0x000000f0; if (t) { t=pal[t>> 4]; pd[2]=(unsigned short)t; }
    t=pack&0x0000000f; if (t) { t=pal[t    ]; pd[3]=(unsigned short)t; }
    t=pack&0xf0000000; if (t) { t=pal[t>>28]; pd[4]=(unsigned short)t; }
    t=pack&0x0f000000; if (t) { t=pal[t>>24]; pd[5]=(unsigned short)t; }
    t=pack&0x00f00000; if (t) { t=pal[t>>20]; pd[6]=(unsigned short)t; }
    t=pack&0x000f0000; if (t) { t=pal[t>>16]; pd[7]=(unsigned short)t; }
    return 0;
  }

  return 1; // Tile blank
}

static int TileFlip(unsigned short *pd,int addr,unsigned short *pal)
{
  unsigned int pack=0; unsigned int t=0;

  pack=*(unsigned int *)(Pico.vram+addr); // Get 8 pixels
  if (pack)
  {
    t=pack&0x000f0000; if (t) { t=pal[t>>16]; pd[0]=(unsigned short)t; }
    t=pack&0x00f00000; if (t) { t=pal[t>>20]; pd[1]=(unsigned short)t; }
    t=pack&0x0f000000; if (t) { t=pal[t>>24]; pd[2]=(unsigned short)t; }
    t=pack&0xf0000000; if (t) { t=pal[t>>28]; pd[3]=(unsigned short)t; }
    t=pack&0x0000000f; if (t) { t=pal[t    ]; pd[4]=(unsigned short)t; }
    t=pack&0x000000f0; if (t) { t=pal[t>> 4]; pd[5]=(unsigned short)t; }
    t=pack&0x00000f00; if (t) { t=pal[t>> 8]; pd[6]=(unsigned short)t; }
    t=pack&0x0000f000; if (t) { t=pal[t>>12]; pd[7]=(unsigned short)t; }
    return 0;
  }
  return 1; // Tile blank
}

#ifndef _ASM_DRAW_C
static void DrawStrip(struct TileStrip *ts)
{
  int tilex=0,dx=0,ty=0,code=0,addr=0,cells;
  int oldcode=-1,blank=-1; // The tile we know is blank
  unsigned short *pal=NULL;

  // Draw tiles across screen:
  tilex=(-ts->hscroll)>>3;
  ty=(ts->line&7)<<1; // Y-Offset into tile
  dx=((ts->hscroll-1)&7)+1;
  cells = ts->cells;
  if(dx != 8) cells++; // have hscroll, need to draw 1 cell more

  for (; cells; dx+=8,tilex++,cells--)
  {
    int zero=0;

    code=Pico.vram[ts->nametab+(tilex&ts->xmask)];
    if (code==blank) continue;
	if (code>>15) { // high priority tile
      *ts->hc++ = code | (dx<<16) | (ty<<25); // cache it
      continue;
    }

	if (code!=oldcode) {
	  oldcode = code;
      // Get tile address/2:
      addr=(code&0x7ff)<<4;
      if (code&0x1000) addr+=14-ty; else addr+=ty; // Y-flip

      pal=PicoCramHigh+((code>>9)&0x30);
	}

    if (code&0x0800) zero=TileFlip(HighCol+24+dx,addr,pal);
    else             zero=TileNorm(HighCol+24+dx,addr,pal);

    if (zero) blank=code; // We know this tile is blank now
  }

  // terminate the cache list
  *ts->hc = 0;
}
#endif

// this is messy
#ifndef _ASM_DRAW_C
static
#endif
void DrawStripVSRam(struct TileStrip *ts, int plane)
{
  int tilex=0,dx=0,ty=0,code=0,addr=0,cell=0,nametabadd=0;
  int oldcode=-1,blank=-1; // The tile we know is blank
  unsigned short *pal=NULL;

  // Draw tiles across screen:
  tilex=(-ts->hscroll)>>3;
  dx=((ts->hscroll-1)&7)+1;
  if(dx != 8) {
    int vscroll, line;
    cell--; // have hscroll, start with negative cell
	// also calculate intial VS stuff
	vscroll=Pico.vsram[plane];

    // Find the line in the name table
    line=(vscroll+Scanline)&ts->line&0xffff; // ts->line is really ymask ..
    nametabadd=(line>>3)<<(ts->line>>24);    // .. and shift[width]
    ty=(line&7)<<1; // Y-Offset into tile
  }

  for (; cell < ts->cells; dx+=8,tilex++,cell++)
  {
    int zero=0;

    if((cell&1)==0) {
	  int line,vscroll;
	  vscroll=Pico.vsram[plane+(cell&~1)];

      // Find the line in the name table
      line=(vscroll+Scanline)&ts->line&0xffff; // ts->line is really ymask ..
      nametabadd=(line>>3)<<(ts->line>>24);    // .. and shift[width]
      ty=(line&7)<<1; // Y-Offset into tile
	}

	code=Pico.vram[ts->nametab+nametabadd+(tilex&ts->xmask)];
    if (code==blank) continue;
	if (code>>15) { // high priority tile
      *ts->hc++ = code | (dx<<16) | (ty<<25); // cache it
      continue;
    }

	if (code!=oldcode) {
	  oldcode = code;
      // Get tile address/2:
      addr=(code&0x7ff)<<4;
      if (code&0x1000) addr+=14-ty; else addr+=ty; // Y-flip

      pal=PicoCramHigh+((code>>9)&0x30);
	}

    if (code&0x0800) zero=TileFlip(HighCol+24+dx,addr,pal);
    else             zero=TileNorm(HighCol+24+dx,addr,pal);

    if (zero) blank=code; // We know this tile is blank now
  }

  // terminate the cache list
  *ts->hc = 0;
}

#ifndef _ASM_DRAW_C
static void DrawLayer(int plane, int *hcache, int maxcells)
{
  struct PicoVideo *pvid=&Pico.video;
  static char shift[4]={5,6,6,7}; // 32,64 or 128 sized tilemaps
  struct TileStrip ts;
  int width, height, ymask;
  int vscroll, htab;

  ts.hc=hcache;
  ts.cells=maxcells;

  // Work out the TileStrip to draw

  // Work out the name table size: 32 64 or 128 tiles (0-3)
  width=pvid->reg[16];
  height=(width>>4)&3; width&=3;

  ts.xmask=(1<<shift[width])-1; // X Mask in tiles
  ymask=(8<<shift[height])-1; // Y Mask in pixels

  // Find name table:
  if (plane==0) ts.nametab=(pvid->reg[2]&0x38)<< 9; // A
  else          ts.nametab=(pvid->reg[4]&0x07)<<12; // B

  htab=pvid->reg[13]<<9; // Horizontal scroll table address
  if ( pvid->reg[11]&2)     htab+=Scanline<<1; // Offset by line
  if ((pvid->reg[11]&1)==0) htab&=~0xf; // Offset by tile
  htab+=plane; // A or B

  // Get horizontal scroll value
  ts.hscroll=Pico.vram[htab&0x7fff];

  if(pvid->reg[11]&4) {
    // shit, we have 2-cell column based vscroll
	// luckily this doesn't happen too often
    ts.line=ymask|(shift[width]<<24); // save some stuff instead of line
	DrawStripVSRam(&ts, plane);
  } else {
    // Get vertical scroll value:
    vscroll=Pico.vsram[plane];

    // Find the line in the name table
    ts.line=(vscroll+Scanline)&ymask;
    ts.nametab+=(ts.line>>3)<<shift[width];

    DrawStrip(&ts);
  }
}


// tstart & tend are tile pair numbers
static void DrawWindow(int tstart, int tend, int prio) // int *hcache
{
  struct PicoVideo *pvid=&Pico.video;
  int tilex=0,ty=0,nametab,code=0;
  int blank=-1; // The tile we know is blank

  // Find name table line:
  if (pvid->reg[12]&1)
  {
    nametab=(pvid->reg[3]&0x3c)<<9; // 40-cell mode
    nametab+=(Scanline>>3)<<6;
  }
  else
  {
    nametab=(pvid->reg[3]&0x3e)<<9; // 32-cell mode
    nametab+=(Scanline>>3)<<5;
  }

  tilex=tstart<<1;
  tend<<=1;

  ty=(Scanline&7)<<1; // Y-Offset into tile

  // check the first tile code
  code=Pico.vram[nametab+tilex];
  // hack: simply assume that the whole window uses same priority
  if((code>>15) != prio) return; // need to check if this causes trouble

  // Draw tiles across screen:
  for (; tilex < tend; tilex++)
  {
    int addr=0,zero=0;
    unsigned short *pal=NULL;

    code=Pico.vram[nametab+tilex];
    if(code==blank) continue;
	/*
    if (code>>15) { // high priority tile
      *hcache++ = code | ((tilex+1)<<19) | (ty<<25); // cache it
      continue;
    }*/

    // Get tile address/2:
    addr=(code&0x7ff)<<4;
    if (code&0x1000) addr+=14-ty; else addr+=ty; // Y-flip

    pal=PicoCramHigh+((code>>9)&0x30);

    if (code&0x0800) zero=TileFlip(HighCol+32+(tilex<<3),addr,pal);
    else             zero=TileNorm(HighCol+32+(tilex<<3),addr,pal);

    if (zero) blank=code; // We know this tile is blank now
  }

  // terminate the cache list
  //*hcache = 0;
}

static void DrawTilesFromCache(int *hc)
{
  int code, addr, zero, ty;
  unsigned short *pal;
  short blank=-1; // The tile we know is blank

  // *ts->hc++ = code | (dx<<16) | (ty<<24); // cache it

  while((code=*hc++)) {
    if((short)code == blank) continue;

    // Get tile address/2:
    addr=(code&0x7ff)<<4;
    ty=(unsigned int)code>>25;
    if (code&0x1000) addr+=14-ty; else addr+=ty; // Y-flip

    pal=PicoCramHigh+((code>>9)&0x30);

    if (code&0x0800) zero=TileFlip(HighCol+24+((code>>16)&0x1ff),addr,pal);
    else             zero=TileNorm(HighCol+24+((code>>16)&0x1ff),addr,pal);

	if(zero) blank=(short)code;
  }
}

static void DrawSprite(unsigned int *sprite,int **hc)
{
  int width=0,height=0;
  int row=0,code=0;
  unsigned short *pal=NULL;
  int tile=0,delta=0;
  int sx, sy;

  // parse the sprite data
  sy=sprite[0];
  height=sy>>24;
  sy=(sy&0x1ff)-0x80; // Y
  width=(height>>2)&3; height&=3;
  width++; height++; // Width and height in tiles

  row=Scanline-sy; // Row of the sprite we are on

  code=sprite[1];
  sx=((code>>16)&0x1ff)-0x78; // X

  if (code&0x1000) row=(height<<3)-1-row; // Flip Y

  tile=code&0x7ff; // Tile number
  tile+=row>>3; // Tile number increases going down
  delta=height; // Delta to increase tile by going right
  if (code&0x0800) { tile+=delta*(width-1); delta=-delta; } // Flip X

  tile<<=4; tile+=(row&7)<<1; // Tile address

  if(code&0x8000) { // high priority - cache it
    *(*hc)++ = (tile<<16)|((code&0x0800)<<5)|((sx<<6)&0x0000ffc0)|((code>>9)&0x30)|((sprite[0]>>24)&0xf);
  } else {
    delta<<=4; // Delta of address
    pal=PicoCramHigh+((code>>9)&0x30); // Get palette pointer

    for (; width; width--,sx+=8,tile+=delta)
    {
      if(sx<=0)   continue;
	  if(sx>=328) break; // Offscreen

      tile&=0x7fff; // Clip tile address
      if (code&0x0800) TileFlip(HighCol+24+sx,tile,pal);
      else             TileNorm(HighCol+24+sx,tile,pal);
    }
  }
}
#endif


static int DrawAllSprites(int *hcache, int maxwidth)
{
  struct PicoVideo *pvid=&Pico.video;
  int table=0;
  int i,u,link=0;
  int sx1seen=0; // sprite with x coord 1 seen
  unsigned char spin[80]; // Sprite index

  table=pvid->reg[5]&0x7f;
  if (pvid->reg[12]&1) table&=0x7e; // Lowest bit 0 in 40-cell mode
  table<<=8; // Get sprite table address/2

  for (i=u=0; u < 80 && i < 21; u++)
  {
    unsigned int *sprite=NULL;
    int code, sx, sy, height;

    sprite=(unsigned int *)(Pico.vram+((table+(link<<2))&0x7ffc)); // Find sprite

    // get sprite info
	code = sprite[0];

	// check if it is on this line
	sy = (code&0x1ff)-0x80;
	height = (((code>>24)&3)+1)<<3;
	if(Scanline < sy || Scanline >= sy+height) goto nextsprite; // no

    // masking sprite?
	sx = (sprite[1]>>16)&0x1ff;
	if(!sx) {
      if(!(rendstatus&1) || (!(rendstatus&1) && sx1seen)) {
        i--; break; // this sprite is not drawn and remaining sprites are masked
	  }
	}
    else if(sx == 1) { rendstatus |= 1; sx1seen = 1; } // masking mode2 (Outrun, Galaxy Force II)

    // check if sprite is not hidden offscreen
	sx -= 0x78; // Get X coordinate + 8
	if(sx <= -8*3 || sx >= maxwidth) goto nextsprite;

	// sprite is good, save it's index
	spin[i++]=(unsigned char)link;

	nextsprite:
	// Find next sprite
    link=(code>>16)&0x7f;
	if(!link) break; // End of sprites
  }

  // Go through sprites backwards:
  for (i-- ;i>=0; i--)
  {
    unsigned int *sprite=NULL;
    sprite=(unsigned int *)(Pico.vram+((table+(spin[i]<<2))&0x7ffc)); // Find sprite

    DrawSprite(sprite,&hcache);
  }

  // terminate cache list
  *hcache = 0;

  return 0;
}


#ifndef _ASM_DRAW_C
static void DrawSpritesFromCache(int *hc)
{
  int code, tile, sx, delta, width;
  unsigned short *pal;

  // *(*hc)++ = (tile<<16)|((code&0x0800)<<5)|(sx<<6)|((code>>9)&0x30)|((sprite[1]>>8)&0xf);

  while((code=*hc++)) {
    pal=PicoCramHigh+(code&0x30); // Get palette pointer
    delta=code&0xf;
    width=delta>>2; delta&=3;
    width++; delta++; // Width and height in tiles
    if (code&0x10000) delta=-delta; // Flip X
	delta<<=4;
	tile=((unsigned int)code>>17)<<1;
	sx=(code<<16)>>22; // sx can be negative (start offscreen), so sign extend

    for (; width; width--,sx+=8,tile+=delta)
    {
      if(sx<=0)   continue;
	  if(sx>=328) break; // Offscreen

      tile&=0x7fff; // Clip tile address
      if (code&0x10000) TileFlip(HighCol+24+sx,tile,pal);
      else              TileNorm(HighCol+24+sx,tile,pal);
    }
  }
}


static void BackFill(int reg7)
{
  unsigned int back=0;
  unsigned int *pd=NULL,*end=NULL;

  // Start with a blank scanline (background colour):
  back=PicoCramHigh[reg7&0x3f];
  back|=back<<16;

  pd= (unsigned int *)(HighCol+32);
  end=(unsigned int *)(HighCol+32+320);

  do { pd[0]=pd[1]=pd[2]=pd[3]=back; pd+=4; } while (pd<end);
}
#endif

static int DrawDisplay()
{
  struct PicoVideo *pvid=&Pico.video;
  int win=0,edge=0,hwind=0,vwind=0;
  int maxw, maxcells;

  if(pvid->reg[12]&1) {
    maxw = 328; maxcells = 40;
  } else {
    maxw = 264; maxcells = 32;
  }

  // Find out if the window is on this line:
  win=pvid->reg[0x12];
  edge=(win&0x1f)<<3;

  if (win&0x80) { if (Scanline>=edge) hwind=1; }
  else          { if (Scanline< edge) hwind=1; }

  if(!hwind) { // we might have a vertical window here 
    win=pvid->reg[0x11];
	edge=win&0x1f;
    if (win&0x80) { if (edge < 20) vwind=1; }
    else          { if (edge)      vwind=1; }
  }

  DrawLayer(1, HighCacheB, maxcells);
  if(hwind)
    DrawWindow(0, maxcells>>1, 0); // HighCacheAW
  else if(vwind) {
    // ahh, we have vertical window
    DrawLayer(0, HighCacheA, (win&0x80) ? edge<<1 : maxcells);
    DrawWindow((win&0x80) ? edge : 0, (win&0x80) ? maxcells>>1 : edge, 0); // HighCacheW
  } else
    DrawLayer(0, HighCacheA, maxcells);
  DrawAllSprites(HighCacheS, maxw);
  
  if(HighCacheB[0])  DrawTilesFromCache(HighCacheB);
  if(hwind)
    DrawWindow(0, maxcells>>1, 1);
  else if(vwind) {
    if(HighCacheA[0]) DrawTilesFromCache(HighCacheA);
	DrawWindow((win&0x80) ? edge : 0, (win&0x80) ? maxcells>>1 : edge, 1);
  } else
    if(HighCacheA[0]) DrawTilesFromCache(HighCacheA);
  if(HighCacheS[0])  DrawSpritesFromCache(HighCacheS);

/*
  if(vwind && HighCacheW[0])
                     DrawTilesFromCache(HighCacheW);
*/
  return 0;
}

static int Skip=0;

int PicoLine(int scan)
{
  if(!scan) {
    // very first line - reset some stuff
	rendstatus = 0;
  }

  if (Skip>0) { Skip--; return 0; } // Skip rendering lines

  Scanline=scan;

  // Draw screen:
  //if (PicoMask&0x02)
  BackFill(Pico.video.reg[7]);
  if (Pico.video.reg[1]&0x40) DrawDisplay();

  //Overlay();

  if (Pico.video.reg[12]&1)
  {
    Skip=PicoScan(Scanline,HighCol+32); // 40-column mode
  }
  else
  {
    // Crop, centre and return 32-column mode
	// notaz: this is not needed here, it is done later
    //memset(HighCol,    0,64); // Left border
    //memset(HighCol+288,0,64); // Right border
    Skip=PicoScan(Scanline,HighCol);
  }

  return 0;
}

