#ifdef HW_FRAME_RENDERER

#include <nds.h>
#include <nds/arm9/console.h>

#include "PicoInt.h"
#include "Timer.h"

#ifdef __MARM__
#define START_ROW  1 // which row of tiles to start rendering at?
#define END_ROW   27 // ..end
#else
#define START_ROW  0
#define END_ROW   28
#endif
#define TILE_ROWS END_ROW-START_ROW

int currpri = 0;
int mapoffset, tileoffset;

static int TileXnormYnorm(int addr) {
	((uint16*)BG_MAP_RAM(0))[addr+tileoffset] = tileoffset;
	// dmaCopy((void*)(Pico.vram+addr),(void*)(BG_TILE_RAM(2)+(mapoffset*8*8)),8*8);
	mapoffset++;
	return 0;
}

static int TileXflipYnorm(int addr) {
	((uint16*)BG_MAP_RAM(0))[addr+tileoffset] = tileoffset;
	// dmaCopy((void*)(Pico.vram+addr),(void*)(BG_TILE_RAM(2)+(mapoffset*8*8)),8*8);
	mapoffset++;
	return 0;
}

static int TileXnormYflip(int addr) {
	((uint16*)BG_MAP_RAM(0))[addr+tileoffset] = tileoffset;
	// dmaCopy((void*)(Pico.vram+addr),(void*)(BG_TILE_RAM(2)+(mapoffset*8*8)),8*8);
	mapoffset++;
	return 0;
}

static int TileXflipYflip(int addr) {
	((uint16*)BG_MAP_RAM(0))[addr+tileoffset] = tileoffset;
	// dmaCopy((void*)(Pico.vram+addr),(void*)(BG_TILE_RAM(2)+(mapoffset*8*8)),8*8);
	mapoffset++;
	return 0;
}

static void PushTile(int addr) {
	// dmaCopy(Pico.vram+addr,(uint16*)BG_TILE_RAM(4)+(tileoffset*8*8),8*8);
	unsigned int pack = 0;
	unsigned int t = 0;
	
	int i;
	uint16 pd[4];
	for(i=0; i < 8; i++, addr+=2) {
		pack=*(unsigned int *)(Pico.vram+addr); // Get 8 pixels
		if(!pack) continue;

		t=pack&0x0000f000; if (t) { t=t>>12; pd[0]=((unsigned short)t)<<8; }
		t=pack&0x00000f00; if (t) { t=t>> 8; pd[0]|=(unsigned short)t; }
		t=pack&0x000000f0; if (t) { t=t>> 4; pd[1]=((unsigned short)t)<<8; }
		t=pack&0x0000000f; if (t) { t=t    ; pd[1]|=(unsigned short)t; }
		t=pack&0xf0000000; if (t) { t=t>>28; pd[2]=((unsigned short)t)<<8; }
		t=pack&0x0f000000; if (t) { t=t>>24; pd[2]|=(unsigned short)t; }
		t=pack&0x00f00000; if (t) { t=t>>20; pd[3]=((unsigned short)t)<<8; }
		t=pack&0x000f0000; if (t) { t=t>>16; pd[3]|=(unsigned short)t; }

		dmaCopy(pd,(uint16*)BG_TILE_RAM(4)+(tileoffset*8*8)+(i*8),8);
	}
}

static void DrawLayerFull(int plane, int planestart, int planeend)
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

	iprintf("\x1b[18;0HWidth: %d\tHeight: %d",width,height);

	// Get vertical scroll value:
	vscroll=Pico.vsram[plane];
	if(vscroll&7) planeend++; // we have vertically clipped tiles due to vscroll, so we need 1 more row

	
	
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


		int tileaddr = ((Pico.vram[nametab_row+(tilex&xmask)])&0x7ff)<<4;
		PushTile(tileaddr);
		
		tileoffset++;
		
		for (; cellc; dx+=8,tilex++,cellc--)
		{
			int code=0,addr=0,zero=0;
			unsigned short *pal=NULL;

			code=Pico.vram[nametab_row+(tilex&xmask)];
			if (code==blank) continue;

			if ((code>>15) != currpri) { // high priority tile
				continue;
			}

			// Get tile address/2:
			addr=(code&0x7ff)<<4;

			// pal=PicoCramHigh+((code>>9)&0x30);

			switch((code>>11)&3) {
				case 0: zero=TileXnormYnorm(dx/8); break;
				case 1: zero=TileXflipYnorm(dx/8); break;
				case 2: zero=TileXnormYflip(dx/8); break;
				case 3: zero=TileXflipYflip(dx/8); break;
			}
			if(zero) blank=code; // We know this tile is blank now
		}


	}

}

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

		// DrawSpriteFull(sprite);
	}
}

static void DrawDisplayFull()
{
	SetupProfile();
	BeginProfile();
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
	DrawLayerFull(1, START_ROW, (maxcolc<<16)|END_ROW);	
	DrawLayerFull(0, START_ROW, (maxcolc<<16)|END_ROW);	
	EndProfile("DrawDisplayFull");
}

static int UpdatePalette()
{
  int c=0;

  // Update palette:
  for (c=0;c<64;c++) BG_PALETTE[c]=(unsigned short)PicoCram(Pico.cram[c]);
  Pico.m.dirtyPal=0;

  return 0;
}

void PicoFrameFull()
{
	// Draw screen
	// iprintf("\x1b[18;0HPicoFrameFull hit");

	tileoffset = 0;
	mapoffset = 0;

	UpdatePalette();
	
	if (Pico.video.reg[1]&0x40) DrawDisplayFull();

	iprintf("\x1b[19;0HTile offset:\t%d     \n",tileoffset);
	iprintf("Map offset:\t%d     ",mapoffset);
}

#endif // HW_FRAME_RENDERER
