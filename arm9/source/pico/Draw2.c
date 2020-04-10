// This is part of Pico Library

// (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


// this is a frame-based renderer, alternative to Dave's line based which is in Draw.c

#include "PicoInt.h"

unsigned short *PicoCramHigh=Pico.cram; // pointer to CRAM buff (0x40 shorts), converted to native device color (works only with 16bit for now)
void (*PicoPrepareCram)()=0;            // prepares PicoCramHigh for renderer to use

#ifdef SW_FRAME_RENDERER

#include <assert.h>
//#include <nds/jtypes.h>
#include <nds/dma.h>
#ifndef __GNUC__
#pragma warning (disable:4706) // Disable assignment within conditional
#endif

#ifdef __MARM__
#define START_ROW  1 // which row of tiles to start rendering at?
#define END_ROW   27 // ..end
#else
#define START_ROW  0
#define END_ROW   28
#endif
#define TILE_ROWS END_ROW-START_ROW

#define USE_CACHE

extern unsigned short *framebuff; // in format (8+320)x(8+224+8) (eights for borders)
#ifdef SW_FRAME_RENDERER
int currpri = 0;
#endif

static int HighCacheA[41*(TILE_ROWS+1)+1+1]; // caches for high layers
static int HighCacheB[41*(TILE_ROWS+1)+1+1];

// stuff available in asm:
#ifdef _ASM_DRAW2_C
void BackFillFull(int reg7);
void DrawLayerFull(int plane, int *hcache, int planestart, int planeend);
void DrawTilesFromCacheF(int *hc);
void DrawWindowFull(int start, int end, int prio);
void DrawSpriteFull(unsigned int *sprite);

#else

static int TileXnormYnorm(unsigned short *pd,int addr,unsigned short *pal) 
{
	unsigned int pack=0; unsigned int t=0, blank = 1;
	int i;

	for(i=8; i; i--, addr+=2, pd += 320+8) {
		pack=*(unsigned int *)(Pico.vram+addr); // Get 8 pixels
		if(!pack) continue;

		t=pack&0x0000f000; if (t) { t=pal[t>>12]; pd[0]=(unsigned short)t; }
		t=pack&0x00000f00; if (t) { t=pal[t>> 8]; pd[1]=(unsigned short)t; }
		t=pack&0x000000f0; if (t) { t=pal[t>> 4]; pd[2]=(unsigned short)t; }
		t=pack&0x0000000f; if (t) { t=pal[t    ]; pd[3]=(unsigned short)t; }
		t=pack&0xf0000000; if (t) { t=pal[t>>28]; pd[4]=(unsigned short)t; }
		t=pack&0x0f000000; if (t) { t=pal[t>>24]; pd[5]=(unsigned short)t; }
		t=pack&0x00f00000; if (t) { t=pal[t>>20]; pd[6]=(unsigned short)t; }
		t=pack&0x000f0000; if (t) { t=pal[t>>16]; pd[7]=(unsigned short)t; }
		blank = 0;
	}

	return blank; // Tile blank?
}

static int TileXflipYnorm(unsigned short *pd,int addr,unsigned short *pal)
{
	unsigned int pack=0; unsigned int t=0, blank = 1;
	int i;

	for(i=8; i; i--, addr+=2, pd += 320+8) {
		pack=*(unsigned int *)(Pico.vram+addr); // Get 8 pixels
		if(!pack) continue;

		t=pack&0x000f0000; if (t) { t=pal[t>>16]; pd[0]=(unsigned short)t; }
		t=pack&0x00f00000; if (t) { t=pal[t>>20]; pd[1]=(unsigned short)t; }
		t=pack&0x0f000000; if (t) { t=pal[t>>24]; pd[2]=(unsigned short)t; }
		t=pack&0xf0000000; if (t) { t=pal[t>>28]; pd[3]=(unsigned short)t; }
		t=pack&0x0000000f; if (t) { t=pal[t    ]; pd[4]=(unsigned short)t; }
		t=pack&0x000000f0; if (t) { t=pal[t>> 4]; pd[5]=(unsigned short)t; }
		t=pack&0x00000f00; if (t) { t=pal[t>> 8]; pd[6]=(unsigned short)t; }
		t=pack&0x0000f000; if (t) { t=pal[t>>12]; pd[7]=(unsigned short)t; }
		blank = 0;
	}
	return blank; // Tile blank?
}

static int TileXnormYflip(unsigned short *pd,int addr,unsigned short *pal)
{
	unsigned int pack=0; unsigned int t=0, blank = 1;
	int i;

	addr+=14;
	for(i=8; i; i--, addr-=2, pd += 320+8) {
		pack=*(unsigned int *)(Pico.vram+addr); // Get 8 pixels
		if(!pack) continue;

		t=pack&0x0000f000; if (t) { t=pal[t>>12]; pd[0]=(unsigned short)t; }
		t=pack&0x00000f00; if (t) { t=pal[t>> 8]; pd[1]=(unsigned short)t; }
		t=pack&0x000000f0; if (t) { t=pal[t>> 4]; pd[2]=(unsigned short)t; }
		t=pack&0x0000000f; if (t) { t=pal[t    ]; pd[3]=(unsigned short)t; }
		t=pack&0xf0000000; if (t) { t=pal[t>>28]; pd[4]=(unsigned short)t; }
		t=pack&0x0f000000; if (t) { t=pal[t>>24]; pd[5]=(unsigned short)t; }
		t=pack&0x00f00000; if (t) { t=pal[t>>20]; pd[6]=(unsigned short)t; }
		t=pack&0x000f0000; if (t) { t=pal[t>>16]; pd[7]=(unsigned short)t; }
		blank = 0;
	}

	return blank; // Tile blank?
}

static int TileXflipYflip(unsigned short *pd,int addr,unsigned short *pal)
{
	unsigned int pack=0; unsigned int t=0, blank = 1;
	int i;

	addr+=14;
	for(i=8; i; i--, addr-=2, pd += 320+8) {
		pack=*(unsigned int *)(Pico.vram+addr); // Get 8 pixels
		if(!pack) continue;

		t=pack&0x000f0000; if (t) { t=pal[t>>16]; pd[0]=(unsigned short)t; }
		t=pack&0x00f00000; if (t) { t=pal[t>>20]; pd[1]=(unsigned short)t; }
		t=pack&0x0f000000; if (t) { t=pal[t>>24]; pd[2]=(unsigned short)t; }
		t=pack&0xf0000000; if (t) { t=pal[t>>28]; pd[3]=(unsigned short)t; }
		t=pack&0x0000000f; if (t) { t=pal[t    ]; pd[4]=(unsigned short)t; }
		t=pack&0x000000f0; if (t) { t=pal[t>> 4]; pd[5]=(unsigned short)t; }
		t=pack&0x00000f00; if (t) { t=pal[t>> 8]; pd[6]=(unsigned short)t; }
		t=pack&0x0000f000; if (t) { t=pal[t>>12]; pd[7]=(unsigned short)t; }
		blank = 0;
	}
	return blank; // Tile blank?
}


// start: (tile_start<<16)|row_start, end: [same]
static void DrawWindowFull(int start, int end, int prio)
{
	struct PicoVideo *pvid=&Pico.video;
	int nametab, nametab_step, trow, tilex, blank=-1, code;
	unsigned short *scrpos = framebuff;
	int tile_start, tile_end; // in cells

	// parse ranges
	tile_start = start>>16;
	tile_end = end>>16;
	start = start<<16>>16;
	end = end<<16>>16;

	// Find name table line:
	if (pvid->reg[12]&1)
	{
		nametab=(pvid->reg[3]&0x3c)<<9; // 40-cell mode
		nametab_step = 1<<6;
	}
	else
	{
		nametab=(pvid->reg[3]&0x3e)<<9; // 32-cell mode
		nametab_step = 1<<5;
	}
	nametab += nametab_step*start;

	// check priority
	code=Pico.vram[nametab+tile_start];
	if ((code>>15) != prio) return; // hack: just assume that whole window uses same priority

	scrpos+=8*328+8;
	scrpos+=8*328*(start-START_ROW);

	// do a window until we reach planestart row
	for(trow = start; trow < end; trow++, nametab+=nametab_step) { // current tile row
		for (tilex=tile_start; tilex<tile_end; tilex++)
		{
			int code,addr,zero=0;
			unsigned short *pal=NULL;

			code=Pico.vram[nametab+tilex];
			if (code==blank) continue;

			// Get tile address/2:
			addr=(code&0x7ff)<<4;

			pal=PicoCramHigh+((code>>9)&0x30);

			switch((code>>11)&3) {
				case 0: zero=TileXnormYnorm(scrpos+(tilex<<3),addr,pal); break;
				case 1: zero=TileXflipYnorm(scrpos+(tilex<<3),addr,pal); break;
				case 2: zero=TileXnormYflip(scrpos+(tilex<<3),addr,pal); break;
				case 3: zero=TileXflipYflip(scrpos+(tilex<<3),addr,pal); break;
			}
			if(zero) blank=code; // We know this tile is blank now
		}

		scrpos += 328*8;
	}
}


static void DrawLayerFull(int plane, int *hcache, int planestart, int planeend)
{
	struct PicoVideo *pvid=&Pico.video;
	static char shift[4]={5,6,6,7}; // 32,64 or 128 sized tilemaps
	int width, height, ymask, htab;
	int nametab, hscroll=0, vscroll, cells;
	unsigned short *scrpos;
	int blank=-1, xmask, nametab_row, trow;

	// parse ranges
	cells = (planeend>>16)-(planestart>>16);
	planestart = planestart<<16>>16;
	planeend = planeend<<16>>16;

	// Work out the Tiles to draw

	htab=pvid->reg[13]<<9; // Horizontal scroll table address
//	if ( pvid->reg[11]&2)     htab+=Scanline<<1; // Offset by line
//	if ((pvid->reg[11]&1)==0) htab&=~0xf; // Offset by tile
	htab+=plane; // A or B

	if(!(pvid->reg[11]&3)) { // full screen scroll
		// Get horizontal scroll value
		hscroll=Pico.vram[htab&0x7fff];
		htab = 0; // this marks that we don't have to update scroll value
	}

	// Work out the name table size: 32 64 or 128 tiles (0-3)
	width=pvid->reg[16];
	height=(width>>4)&3; width&=3;

	xmask=(1<<shift[width ])-1; // X Mask in tiles
	ymask=(1<<shift[height])-1; // Y Mask in tiles

	// Find name table:
	if (plane==0) nametab=(pvid->reg[2]&0x38)<< 9; // A
	else          nametab=(pvid->reg[4]&0x07)<<12; // B

	scrpos = framebuff;
	scrpos+=8*328*(planestart-START_ROW);

	// Get vertical scroll value:
	vscroll=Pico.vsram[plane];
	scrpos+=(8-(vscroll&7))*328;
	if(vscroll&7) planeend++; // we have vertically clipped tiles due to vscroll, so we need 1 more row

	*hcache++ = 8-(vscroll&7); // push y-offset to tilecache


	for(trow = planestart; trow < planeend; trow++) { // current tile row
		int cellc=cells,tilex,dx;

		// Find the tile row in the name table
		//ts.line=(vscroll+Scanline)&ymask;
		//ts.nametab+=(ts.line>>3)<<shift[width];
		nametab_row = nametab + (((trow+(vscroll>>3))&ymask)<<shift[width]); // pointer to nametable entries for this row

		// update hscroll if needed
		if(htab) {
			int htaddr=htab+(trow<<4);
			if(trow) htaddr-=(vscroll&7)<<1;
			hscroll=Pico.vram[htaddr&0x7fff];
		}

		// Draw tiles across screen:
		tilex=(-hscroll)>>3;
		dx=((hscroll-1)&7)+1;
		if(dx != 8) cellc++; // have hscroll, do more cells

		for (; cellc; dx+=8,tilex++,cellc--)
		{
			int code=0,addr=0,zero=0;
			unsigned short *pal=NULL;

			code=Pico.vram[nametab_row+(tilex&xmask)];
			if (code==blank) continue;

#ifdef USE_CACHE
			if (code>>15) { // high priority tile
				*hcache++ = code|(dx<<16)|(trow<<27); // cache it
#else
			if ((code>>15) != currpri) {
#endif
				continue;
			}

			// Get tile address/2:
			addr=(code&0x7ff)<<4;

			pal=PicoCramHigh+((code>>9)&0x30);

			switch((code>>11)&3) {
				case 0: zero=TileXnormYnorm(scrpos+dx,addr,pal); break;
				case 1: zero=TileXflipYnorm(scrpos+dx,addr,pal); break;
				case 2: zero=TileXnormYflip(scrpos+dx,addr,pal); break;
				case 3: zero=TileXflipYflip(scrpos+dx,addr,pal); break;
			}
			if(zero) blank=code; // We know this tile is blank now
		}

		scrpos += 328*8;
	}

	*hcache = 0; // terminate cache
}


static void DrawTilesFromCacheF(int *hc)
{
	int code, addr, zero = 0;
	unsigned int prevy=0xFFFFFFFF;
	unsigned short *pal;
	short blank=-1; // The tile we know is blank
	unsigned short *scrpos = framebuff, *pd = 0;

	// *hcache++ = code|(dx<<16)|(trow<<27); // cache it
	scrpos+=(*hc++)*328 - START_ROW*328*8;

	while((code=*hc++)) {
		if((short)code == blank) continue;

		// y pos
		if(((unsigned)code>>27) != prevy) {
			prevy = (unsigned)code>>27;
			pd = scrpos + prevy*328*8;
		}

		// Get tile address/2:
		addr=(code&0x7ff)<<4;
		pal=PicoCramHigh+((code>>9)&0x30);

		switch((code>>11)&3) {
			case 0: zero=TileXnormYnorm(pd+((code>>16)&0x1ff),addr,pal); break;
			case 1: zero=TileXflipYnorm(pd+((code>>16)&0x1ff),addr,pal); break;
			case 2: zero=TileXnormYflip(pd+((code>>16)&0x1ff),addr,pal); break;
			case 3: zero=TileXflipYflip(pd+((code>>16)&0x1ff),addr,pal); break;
		}

		if(zero) blank=(short)code;
	}
}


// sx and sy are coords of virtual screen with 8pix borders on top and on left
static void DrawSpriteFull(unsigned int *sprite)
{
	int width=0,height=0;
	unsigned short *pal=NULL;
	int tile,code,tdeltax,tdeltay;
	unsigned short *scrpos;
	int sx, sy;

	sy=sprite[0];
	height=sy>>24;
	sy=(sy&0x1ff)-0x78; // Y
	width=(height>>2)&3; height&=3;
	width++; height++; // Width and height in tiles

	code=sprite[1];
	sx=((code>>16)&0x1ff)-0x78; // X

	tile=code&0x7ff; // Tile number
	tdeltax=height; // Delta to increase tile by going right
	tdeltay=1;      // Delta to increase tile by going down
	if (code&0x0800) { tdeltax=-tdeltax; tile+=height*(width-1); } // Flip X
	if (code&0x1000) { tdeltay=-tdeltay; tile+=height-1; } // Flip Y

	//delta<<=4; // Delta of address
	pal=PicoCramHigh+((code>>9)&0x30); // Get palette pointer

	// goto first vertically visible tile
	while(sy <= START_ROW*8) { sy+=8; tile+=tdeltay; height--; }

	scrpos = framebuff;
	scrpos+=(sy-START_ROW*8)*328;

	for (; height > 0; height--, sy+=8, tile+=tdeltay)
	{
		int w = width, x=sx, t=tile;

		if(sy >= END_ROW*8+8) return; // offscreen

		for (; w; w--,x+=8,t+=tdeltax)
		{
			if(x<=0)   continue;
			if(x>=328) break; // Offscreen

			t&=0x7fff; // Clip tile address
			switch((code>>11)&3) {
				case 0: TileXnormYnorm(scrpos+x,t<<4,pal); break;
				case 1: TileXflipYnorm(scrpos+x,t<<4,pal); break;
				case 2: TileXnormYflip(scrpos+x,t<<4,pal); break;
				case 3: TileXflipYflip(scrpos+x,t<<4,pal); break;
			}
		}

		scrpos+=8*328;
	}
}
#endif


static void DrawAllSpritesFull(int prio, int maxwidth)
{
	struct PicoVideo *pvid=&Pico.video;
	int table=0,maskrange=0;
	int i,u,link=0;
	unsigned char spin[80]; // Sprite index
	int y_min=START_ROW*8, y_max=END_ROW*8; // for a simple sprite masking

	table=pvid->reg[5]&0x7f;
	if (pvid->reg[12]&1) table&=0x7e; // Lowest bit 0 in 40-cell mode
	table<<=8; // Get sprite table address/2

	for (i=u=0; u < 80; u++)
	{
		unsigned int *sprite=NULL;
		int code, code2, sx, sy, height;

		sprite=(unsigned int *)(Pico.vram+((table+(link<<2))&0x7ffc)); // Find sprite

		// get sprite info
		code = sprite[0];

		// check if it is not hidden vertically
		sy = (code&0x1ff)-0x80;
		height = (((code>>24)&3)+1)<<3;
		if(sy+height <= y_min || sy > y_max) goto nextsprite;

		// masking sprite?
		code2=sprite[1];
		sx = (code2>>16)&0x1ff;
		if(!sx) {
			int to = sy+height; // sy ~ from
			if(maskrange) {
				// try to merge with previous range
				if((maskrange>>16)+1 >= sy && (maskrange>>16) <= to && (maskrange&0xffff) < sy) sy = (maskrange&0xffff);
				else if((maskrange&0xffff)-1 <= to && (maskrange&0xffff) >= sy && (maskrange>>16) > to) to = (maskrange>>16);
			}
			// support only very simple masking (top and bottom of screen)
			if(sy <= y_min && to+1 > y_min) y_min = to+1;
			else if(to >= y_max && sy-1 < y_max) y_max = sy-1;
			else maskrange=sy|(to<<16);

			goto nextsprite;
		}

		// priority
		if(((code2>>15)&1) != prio) goto nextsprite; // wrong priority

		// check if sprite is not hidden horizontally
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

		DrawSpriteFull(sprite);
	}
}

#ifndef _ASM_DRAW2_C
static void BackFillFull(int reg7)
{
	unsigned int back, i;
	//unsigned int *p=(unsigned int *)framebuff;

	// Start with a background color:
	back=PicoCramHigh[reg7&0x3f];
	back|=back<<16;

	/*for(i = (8+320)*(8+(END_ROW-START_ROW)*8)/8; i; i--) {
		*p++ = back; // do 8 pixels per iteration
		*p++ = back;
		*p++ = back;
		*p++ = back;
	}*/
	dmaFillWords(back, framebuff, (8+320)*(8+(END_ROW-START_ROW)*8));
}
#endif

static void DrawDisplayFull()
{
	struct PicoVideo *pvid=&Pico.video;
	int win, edge=0, vwind=0, hwin=0;
	int planestart=START_ROW, planeend=END_ROW; // plane A start/end when window shares display with plane A (in tile rows or columns)
	int winstart=START_ROW, winend=END_ROW;     // same for window
	int maxw, maxcolc; // max width and col cells

	if(pvid->reg[12]&1) {
		maxw = 328; maxcolc = 40;
	} else {
		maxw = 264; maxcolc = 32;
	}

	// horizontal window?
	if((win=pvid->reg[0x12])) {
		hwin=1; // hwindow shares display with plane A
		if(win == 0x80) {
			// fullscreen window
			hwin=2;
		} else if(win < 0x80) {
			// window on the top
			planestart = winend = win&0x1f;
			if(planestart <= START_ROW) hwin=0; // window not visible in our drawing region
			if(planestart >= END_ROW)   hwin=2;
		} else if(win > 0x80) {
			// window at the bottom
			planeend = winstart = (win&0x1f);
			if(planeend >= END_ROW) hwin=0;
		}
	}

	// check for vertical window, but only if there is no horizontal
	if(!hwin) {
		win=pvid->reg[0x11];
		edge=win&0x1f;
		if (win&0x80) {
			if(!edge) hwin=2;
			else if(edge < (maxcolc>>1)) {
				// window is on the right
				vwind=1;
				planeend=winstart=edge<<1;
				planestart=0; winend=maxcolc;
			}
		} else {
			if(edge >= (maxcolc>>1)) hwin=2;
			else if(edge) {
				// window is on the left
				vwind=1;
				winend=planestart=edge<<1;
				winstart=0; planeend=maxcolc;
			}
		}
	}

	currpri = 0;
	DrawLayerFull(1, HighCacheB, START_ROW, (maxcolc<<16)|END_ROW);
	if(hwin == 2) {
		// fullscreen window
		DrawWindowFull(START_ROW, (maxcolc<<16)|END_ROW, 0);
		HighCacheA[1] = 0;
	} else if(hwin) {
		// both window and plane A visible, window is horizontal
		DrawLayerFull(0, HighCacheA, planestart, (maxcolc<<16)|planeend);
		DrawWindowFull(winstart, (maxcolc<<16)|winend, 0);
	} else if(vwind) {
		// both window and plane A visible, window is vertical
		DrawLayerFull(0, HighCacheA, (planestart<<16)|START_ROW, (planeend<<16)|END_ROW);
		DrawWindowFull((winstart<<16)|START_ROW, (winend<<16)|END_ROW, 0);
	} else
		// fullscreen plane A
		DrawLayerFull(0, HighCacheA, START_ROW, (maxcolc<<16)|END_ROW);
	DrawAllSpritesFull(0, maxw);

#ifdef USE_CACHE
	if(HighCacheB[1]) DrawTilesFromCacheF(HighCacheB);
	if(HighCacheA[1]) DrawTilesFromCacheF(HighCacheA);
	if(hwin == 2) {
		// fullscreen window
		DrawWindowFull(START_ROW, (maxcolc<<16)|END_ROW, 1);
	} else if(hwin) {
		// both window and plane A visible, window is horizontal
		DrawWindowFull(winstart, (maxcolc<<16)|winend, 1);
	} else if(vwind) {
		// both window and plane A visible, window is vertical
		DrawWindowFull((winstart<<16)|START_ROW, (winend<<16)|END_ROW, 1);
	}
#else
	currpri = 1;
	// TODO
#endif
	DrawAllSpritesFull(1, maxw);
}

void PicoFrameFull()
{
	// prepare cram?
	if(PicoPrepareCram) PicoPrepareCram();
	
	// Draw screen:
	BackFillFull(Pico.video.reg[7]);
	if (Pico.video.reg[1]&0x40) DrawDisplayFull();
}

#endif // SW_FRAME_RENDERER
