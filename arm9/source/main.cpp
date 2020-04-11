#define DEBUG 0
#define V_OFFSET 12
#define H_OFFSET 8
#define MAX_FRAMESKIP 4
#define DESIRED_FPS 60

#include <nds.h>
#include <nds/bios.h>
#include <nds/arm9/console.h>
#include <fat.h>
#include <stdio.h>
#include <sys/unistd.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include "nitrofs.h"
#include "libdsp/CmnDspProcess.h"
#include "libdsp/CtrDspProcess.h"
#include "libdsp/dsp_fifo.h"
#include "libdsp/dsp_pipe.h"

#include "pico/PicoInt.h"
#include "file.h"
#include "file_browse.h"

using namespace std;

#ifdef USE_ZLIB
#include "zlib.h"
#endif

#include <sys/stat.h>
#define DEVICE_TYPE_SCSD 0x44534353
#define DEVICE_TYPE_SCCF 0x46434353
#define OPERA_UNLOCK *((vuint16*)0x08240000)
#define OPERA_RAM ((vuint16*)0x09000000)
#define SC_UNLOCK ((vuint16*)0x09fffffe)

unsigned short *framebuff = 0;

#ifdef SW_FRAME_RENDERER
unsigned short realbuff[(8+320)*(8+224+8)];
#endif

static unsigned char *RomData=NULL;
static unsigned int RomSize=0;

static bool UsingAppendedRom = false;
// separate from UsingAppendedRom, we need to track if we're using
// GBAROM as writable memory
static bool UsingExtendedMemory = false;

FILE *romfile;

int choosingfile = 1;

u32 dsFrameCount = 0;
u32 pdFrameCount = 0;
u32 FPS = 0;
int frameCountForFrameSkip = 0;

u32 xdxval = 320;
u32 ydyval = 300;

short scalemode = 0;
bool width256 = false;
bool currentWidth = false;

#if defined(SW_FRAME_RENDERER) || defined(SW_SCAN_RENDERER)
static unsigned short cram_high[0x40];

void UpdatePalette()
{
  int c=0;

  // Update palette:
  for (c=0;c<64;c++) cram_high[c]=(unsigned short)PicoCram(Pico.cram[c]);
  Pico.m.dirtyPal=0;
}
#endif

/*void bgupdate()
{
	s16 c = cosLerp(0) >> 4;
	REG_BG3PA = ( c * (xdxval))>>8;
	REG_BG3PD = ( c * (ydyval))>>8;
	iprintf("\x1b[16;0Hxdxval: %d    \n",xdxval);
	iprintf("ydyval: %d    ",ydyval);
}*/

void PrintRegion()
{
	iprintf("\n");
	switch(Pico.m.hardware)
	{
		case 0xe0:
			iprintf("Europe\n");
			break;
		case 0xa0:
			iprintf("USA\n");
			break;
		case 0x60:
			iprintf("Japan PAL\n");
			break;
		case 0x20:
			iprintf("Japan NTSC\n");
			break;
		default:
			iprintf("Unknown\n");
			break;
	}
}

#ifdef ARM9_SOUND
void InitSound()
{
	if(PsndOut)
		free(PsndOut);

	PsndOut = (short *)malloc(PsndLen);
}
#endif

#ifdef USE_ZLIB
#define CHUNK 512

int compressSaveState(void)
{
	FILE *source = fopen("testsave.nc", "rb");
	FILE *dest = fopen("testsave.z", "wb");
	int ret, flush;
	int totalbytes = 0;
	int maxchunk;
	unsigned have;
	z_stream strm;
	// char in[CHUNK];
	// char out[CHUNK];
	char *in = malloc(CHUNK*sizeof(char));
	char *out = malloc(CHUNK*sizeof(char));

	struct Cyclone *cpustate = &PicoCpu;
	struct Pico *emustate = &Pico;

	// original routine
	/*
	PmovFile = fopen(saveFname, "wb");
	res = fwrite(cpustate,1,sizeof(struct Cyclone),(FILE *) PmovFile);
	res = (res != sizeof(PicoCpu) ? -1 : 0);
	res = fwrite(emustate,1,sizeof(struct Pico),(FILE *) PmovFile);
	res = (res != sizeof(Pico) ? -1 : 0);
	fclose((FILE *) PmovFile);
	*/
	
	// compressed routine
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	ret = deflateInit(&strm, Z_BEST_COMPRESSION);
	if(ret != Z_OK) {
		return ret;
	}

// original code for compressing state without using an intermediate file
#ifdef NOT_USED
	// compress all of the save state
	do {
		if((totalbytes + CHUNK) <= sizeof(struct Cyclone)) {
			maxchunk = CHUNK;
		}
		else {
			maxchunk = sizeof(struct Cyclone)-totalbytes;
		}
		totalbytes += maxchunk;
		memcpy(in,cpustate, maxchunk);
		strm.avail_in = maxchunk;
		strm.next_in = (unsigned char *) in;
		do {
			strm.avail_out = CHUNK;
			strm.next_out = (unsigned char *) out;
			ret = deflate(&strm, Z_NO_FLUSH);    /* no bad return value */
			have = CHUNK - strm.avail_out;
			if(fwrite(out, 1, have, dest) != have) {
				(void)deflateEnd(&strm);
				return Z_ERRNO;
			}
		} while (strm.avail_out == 0);
	} while (totalbytes < sizeof(struct Cyclone));

	totalbytes = 0;

	do {
		if((totalbytes + CHUNK) <= sizeof(struct Pico)) {
			maxchunk = CHUNK;
			flush = Z_NO_FLUSH;
		}
		else {
			maxchunk = sizeof(struct Pico)-totalbytes;
			flush = Z_FINISH;
		}
		totalbytes += maxchunk;
		memcpy(in, emustate, maxchunk);
		strm.avail_in = maxchunk;
		strm.next_in = (unsigned char *) in;
		do {
			strm.avail_out = CHUNK;
			strm.next_out = (unsigned char *) out;
			ret = deflate(&strm, flush);    /* no bad return value */
			have = CHUNK - strm.avail_out;
			if (fwrite(out, 1, have, dest) != have) {
				(void)deflateEnd(&strm);
				return Z_ERRNO;
			}
		} while (strm.avail_out == 0);
	} while (totalbytes < sizeof(struct Pico));
#endif

	/* compress until end of file */
    do {
        strm.avail_in = fread(in, 1, CHUNK, source);

		flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
        strm.next_in = (unsigned char *) in;

        /* run deflate() on input until output buffer not full, finish
           compression if all of source has been read in */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = (unsigned char *) out;
            ret = deflate(&strm, flush);    /* no bad return value */
            have = CHUNK - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have) {
                (void)deflateEnd(&strm);
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);

        /* done when last data in file processed */
    } while (flush != Z_FINISH);
	(void)deflateEnd(&strm);
	free(in);
	free(out);
	fclose(source);
	fclose(dest);
	return Z_OK;
}

int decompressSaveState(void)
{
	FILE *source = fopen("testsave.z", "rb");
	FILE *dest = fopen("testsave.uz", "wb");
	int ret;
	unsigned have;
	z_stream strm;
	// char in[CHUNK];
	// char out[CHUNK];
	char *in = malloc(CHUNK*sizeof(char));
	char *out = malloc(CHUNK*sizeof(char));

	// compressed routine
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;
	ret = inflateInit(&strm);
	if(ret != Z_OK) {
		return ret;
	}

	/* decompress until deflate stream ends or end of file */
	do {
		strm.avail_in = fread(in, 1, CHUNK, source);
		if (strm.avail_in == 0)
			break;
		strm.next_in = (unsigned char *) in;

		/* run inflate() on input until output buffer not full */
		do {
			strm.avail_out = CHUNK;
			strm.next_out = (unsigned char *) out;
			ret = inflate(&strm, Z_NO_FLUSH);
			switch (ret) {
				case Z_NEED_DICT:
					ret = Z_DATA_ERROR;     /* and fall through */
				case Z_DATA_ERROR:
				case Z_MEM_ERROR:
					(void)inflateEnd(&strm);
					return ret;
			}
			have = CHUNK - strm.avail_out;
			if (fwrite(out, 1, have, dest) != have ) {
				(void)inflateEnd(&strm);
				return Z_ERRNO;
            }
		} while (strm.avail_out == 0);

		/* done when inflate() says it's done */
	} while (ret != Z_STREAM_END);

	/* clean up and return */
	(void)inflateEnd(&strm);
	free(in);
	free(out);
	fclose(source);
	fclose(dest);

	return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}
#endif // USE_ZLIB

int saveLoadGame(int load, int sram)
{
	int i;
	int res = 0;
	FILE *PmovFile;
	if(!UsingAppendedRom && !fileName) return -1;

	// make save filename
	char saveFname[256];
	if(!UsingAppendedRom) {
		strcpy(saveFname, fileName);
		if(saveFname[strlen(saveFname)-4] == '.') saveFname[strlen(saveFname)-4] = 0;
		strcat(saveFname, sram ? ".srm" : ".pds");
		// iprintf("\x1b[0;0HSavename: %s\n",saveFname);
	}

	if(sram) {
		if(load) {
			if(UsingAppendedRom) {
				for(i = 0; i < (int)(SRam.end - SRam.start + 1); i++) {
					((uint8*)(SRam.data))[i] = SRAM[i];
				}
				// memcpy(SRam.data,SRAM,SRam.end - SRam.start + 1);
			}
			else {
				PmovFile = fopen(saveFname, "rb");
				if(!PmovFile) return -1;
				fread(SRam.data, 1, SRam.end - SRam.start + 1, (FILE *) PmovFile);
				fclose((FILE *) PmovFile);
			}
		} else {
			// sram save needs some special processing
			int sram_size = SRam.end-SRam.start+1;
			// see if we have anything to save
			for(; sram_size > 0; sram_size--)
				if(SRam.data[sram_size-1]) break;

			if(sram_size) {
				if(UsingAppendedRom) {
					for(i = 0; i < (int)(SRam.end - SRam.start + 1); i++) {
						SRAM[i] = ((uint8*)(SRam.data))[i];
					}
					// memcpy(SRAM,SRam.data,sram_size);
				}
				else {
					PmovFile = fopen(saveFname, "wb");
					res = fwrite(SRam.data, 1, sram_size, (FILE *) PmovFile);
					res = (res != sram_size) ? -1 : 0;
					fclose((FILE *) PmovFile);
				}
			}
		}
		PmovFile = 0;
		return res;
	}
	else { // We're dealing with a save state
		struct Cyclone *cpustate = &PicoCpu;
		struct Pico *emustate = &Pico;
		if(load) { // load save state
			PmovFile = fopen(saveFname, "rb");
			if(!PmovFile) return -1;
			fread(cpustate, 1, sizeof(struct Cyclone), (FILE *) PmovFile);
			fread(emustate, 1, sizeof(struct Pico), (FILE *) PmovFile);
			fclose((FILE *) PmovFile);
		}
		else { // write save state
			// Write out PicoCpu and Pico
			PmovFile = fopen(saveFname, "wb");
			res = fwrite(cpustate,1,sizeof(struct Cyclone),(FILE *) PmovFile);
			res = (res != sizeof(PicoCpu) ? -1 : 0);
			res = fwrite(emustate,1,sizeof(struct Pico),(FILE *) PmovFile);
			res = (res != sizeof(Pico) ? -1 : 0);
			fclose((FILE *) PmovFile);
		}
	}
	
	return 0;
}

int32 cx=32,cy=16;
bool sVibrate = false;

void dosVibrate()
{
	s16 c = cosLerp(0) >> 4;
	int xSize = 316+sVibrate;
	if (width256) {
		xSize = 256;
	}
	switch(scalemode) {
	case 0: // aspect
		REG_BG3PA = ( c * (xSize))>>8;
		REG_BG3PD = ( c * (316+sVibrate))>>8;
		break;
	case 1: // 1:1
	default:
		break;
	}
	sVibrate = !sVibrate;
}

void ChangeScaleMode()
{
	s16 c = cosLerp(0) >> 4;
	int xSize = 316;
	//int xPos = 0;
	if (width256) {
		xSize = 256;
		//#ifdef SW_SCAN_RENDERER
		//xPos = 32;
		//#endif
	}
	switch(scalemode) {
	case 0: // aspect
		REG_BG3PA = ( c * (xSize))>>8;
		REG_BG3PD = ( c * (316+sVibrate))>>8;
		REG_BG3X  = 0;
		REG_BG3Y  = (-6) << 8;
		break;
	case 1: // 1:1
		REG_BG3PA = ( c * (256))>>8;
		REG_BG3PD = ( c * (256))>>8;
		REG_BG3X = cx << 8;
		REG_BG3Y = cy << 8;
		break;
	default:
		break;
	}
}

void ChangeScreenPosition()
{
	scanKeys();
	uint32 keysPressed;
	while(keysHeld() & KEY_R) {
		keysPressed = keysDownRepeat();
		if(keysPressed & KEY_UP) {
			cy -= 4;
		}
		else if(keysPressed & KEY_DOWN) {
			cy += 4;
		}

		if(keysPressed & KEY_RIGHT) {
			cx += 4;
		}
		else if(keysPressed & KEY_LEFT) {
			cx -= 4;
		}

		if(cy < 0) {
			cy = 0;
		}
		else if(cy > 32) {
			cy = 32;
		}

		if(cx < 0) {
			cx = 0;
		}
		else if(cx > 60) {
			cx = 60;
		}
		
		REG_BG3X = cx << 8;
		REG_BG3Y = cy << 8;
		// iprintf("\x1b[17;0Hcy: %d  \n",cy);
		// iprintf("cx: %d  ",cx);
		scanKeys();
		swiWaitForVBlank();
	}
}

void ConvertToGrayscale()
{
	int i,j;
	u8 b,g,r,max,min;
	for(i = 0; i < 224; i++) {
		for(j = 0; j < 320; j++) {
			// ABBBBBGGGGGRRRRR
			// ABBBBB
			// 011111 = 31
			b = ((BG_GFX[(i*512)+j])>>10)&31;
			g = ((BG_GFX[(i*512)+j])>>5)&31;
			r = (BG_GFX[(i*512)+j])&31;
			// Value decomposition of hsv
			max = (b > g) ? b : g;
			max = (max > r) ? max : r;

			// Desaturate
			min = (b < g) ? b : g;
			min = (min < r) ? min : r;
			max = (max + min) / 2;

			// Weighted average - very slow
			// max = (u8)((0.3*r) + (0.59*g) + (0.11*b));
			BG_GFX[(i*512)+j] = 32768|(max<<10)|(max<<5)|(max);
		}
	}
}

static int SaveStateMenu()
{
	int position = 0;
	
	// TODO: Compressed save states for appended ROM mode
	if(UsingAppendedRom) {
		return 0;
	}

	ConvertToGrayscale();
	consoleClear();
	iprintf("\x1b[1;10HLoad State");
	iprintf("\x1b[2;10HSave State");
	while(1) {
		if(!position) {
			iprintf("\x1b[1;7H-> ");
			iprintf("\x1b[2;7H   ");
		}
		else {
			iprintf("\x1b[1;7H   ");
			iprintf("\x1b[2;7H-> ");
		}
		scanKeys();
		if((keysDown() & KEY_DOWN) || (keysDown() & KEY_UP)) {
			position = !position;
		}
		if(keysDown() & KEY_B) {
			consoleClear();
			return 0;
		}
		if(keysDown() & KEY_A) {
			consoleClear();
			if(position) { // save state
				iprintf("Saving state...");
				saveLoadGame(0,0);
				iprintf("DONE!\n");
				return 0;
			}
			else { // load state
				iprintf("Loading state...");
				saveLoadGame(1,0);
				iprintf("DONE!");
				return 0;
			}
		}
	}
	return -1;
}

static int DoFrame()
{
	if(DEBUG)
		iprintf("HIT DOFRAME\n");
	int pad=0;
	// char map[8]={0,1,2,3,5,6,4,7}; // u/d/l/r/b/c/a/start

	scanKeys();
	int keysPressed = keysHeld();
	int kd = keysDown();

	if (keysPressed & KEY_UP) pad|=1<<0;
	if (keysPressed & KEY_DOWN) pad|=1<<1;
	if (keysPressed & KEY_LEFT) pad|=1<<2;
	if (keysPressed & KEY_RIGHT) pad|=1<<3;
	if (keysPressed & KEY_B) pad|=1<<4;
	if (keysPressed & KEY_A) pad|=1<<5;
	if (keysPressed & KEY_Y) pad|=1<<6;
	if (keysPressed & KEY_START) pad|=1<<7;

	if (keysPressed & KEY_X) {
		SaveStateMenu();
	}

	if ((keysPressed & KEY_R) && (scalemode == 1)) {
		ChangeScreenPosition();
	}

	if (keysPressed & KEY_L) {
		if(kd & KEY_L) {
			scalemode = (scalemode + 1) % 2;
			ChangeScaleMode();
		}
	}
	
	if (kd & KEY_SELECT) {
		choosingfile = 2;
	}

	PicoPad[0]=pad;

	width256 = (((Pico.video.reg[12] & 1) == 0) /*&& !(PicoIn.AHW & PAHW_SMS)*/);
	PicoFrame();

#ifdef ARM9_SOUND
	if(PsndOut)
		soundPlaySample((const void *)PsndOut, SoundFormat_16Bit, PsndLen, PsndRate, 127, 64, false, 0);
#endif

	// FramesDone++;
	// pdFrameCount++;
	return 0;
}

#ifdef SW_SCAN_RENDERER
static int EmulateScanBG3(unsigned int scan,unsigned short *sdata)
{
	// BG_GFX is in the form:
	// ABBBBBGGGGGRRRRR
	// 1111110000000000

	// 12345678
	//
	// BBBBGGGGRRRR
	/*
	for(int i = 0; i < 320; i++) {
		BG_GFX[(512*scan)+i]=sdata[i];
	}
	*/

	// iprintf("\x1b[17;0HScanline: %d        ",scan);
	// scan goes from 0 - 223
	
	/*
	int i = 0;
	for(i = 0; i < 320*2; i++) {
		sdata[i] = PicoCram(((u16*)sdata)[i]);
	}
	*/
	dmaCopy(sdata,BG_GFX+(512*scan),320*2);
	// memcpy(BG_GFX+(512*scan),sdata,320);
	// dmaCopy(sdata,VRAM_A_MAIN_BG_0x6000000+(512*scan),320*2);
	/*
	if(scan == 223) {
		memset(BG_GFX+(512*224),64512,512*16);
	}
	*/	
	return 0;
}
#endif

// Original (deprecated) Callback for scanline data:
/*
static int EmulateScan(unsigned int scan,unsigned short *sdata)
{
	int len=0;
	unsigned short *ps=NULL,*end=NULL;
	unsigned short *pd=NULL;
	// unsigned short temp;
	int xpitch=0;

	if ((scan&3)==1) return 0;
	scan+=scan<<1; scan>>=2; // Reduce size to 75%

	// scan+=Targ.offset;
	// if ((int)scan< Targ.top) return 0; // Out of range
	if ((int)scan>=SCREEN_HEIGHT) return 0; // Out of range

	// pd=Targ.screen+scan*GXDisp.cbyPitch;
	pd = VRAM_A+((scan+V_OFFSET)*SCREEN_WIDTH)+H_OFFSET;

	len=240;
	xpitch=1;
	ps=sdata; end=ps+320;

	// Reduce 4 pixels into 3
	do
	{
		// Old code used with original cram algorithm
		// *(unsigned short *)pd=RGB15((ps[0]>>11)&30,(ps[0]&1920)>>6,(ps[0]&30)); pd+=xpitch;
		// temp = (unsigned short)((ps[1]+ps[2])>>1);
		// *(unsigned short *)pd=RGB15((temp>>11)&30,(temp&1920)>>6,(temp&30)); pd+=xpitch;
		// *(unsigned short *)pd=RGB15((ps[3]>>11)&30,(ps[3]&1920)>>6,(ps[3]&30)); pd+=xpitch;

		*(unsigned short *)pd=ps[0]; pd+=xpitch;
		*(unsigned short *)pd=(unsigned short)((ps[1]+ps[2])>>1); pd+=xpitch;
		*(unsigned short *)pd=ps[3]; pd+=xpitch;

		ps+=4;
	}
	while (ps<end);

	return 0;
}
*/

static int DrawFrame()
{
	if(DEBUG)
		iprintf("HIT DRAWFRAME\n");

	// Now final frame is drawn:
#ifdef SW_FRAME_RENDERER
	framebuff = realbuff;
#endif

#ifdef SW_SCAN_RENDERER
	UpdatePalette();
	PicoScan=EmulateScanBG3; // Setup scanline callback
#endif

	DoFrame();

#ifdef SW_FRAME_RENDERER
	unsigned int scan;

	for(scan = 223+8; scan > 8; scan--) {
		dmaCopy(realbuff+(328*scan)+8,BG_GFX+(512*(scan-8)),320*2);
	}
#endif

#ifdef SW_SCAN_RENDERER
	PicoScan=NULL;
#endif

	frameCountForFrameSkip -= 2;
	if (frameCountForFrameSkip < 0) frameCountForFrameSkip = 0;
	pdFrameCount++;
	return 0;
}


void EmulateFrame()
{
	if (choosingfile || RomData==NULL) {
		//iprintf("YOUR ROM DATA IS NULL THAT IS NOT GOOD\n");
		// swiDelay(100000);
		return;
	}

	if(DEBUG)
		iprintf("HIT EMULATEFRAME\n");

	// iprintf("\x1b[19;0HFPS: %d     \n",FPS);

	for (int i=0;i<=frameCountForFrameSkip;i++) {
		PicoSkipFrame = 1;
		DoFrame(); // Frame skip if needed
		// if(PsndOut)
		//		playSound(&picosound);
	}
	PicoSkipFrame = 0;

	DrawFrame();

	if(DEBUG)
		iprintf("LEAVING EMULATEFRAME\n");

	return;
}

void processtimer()
{
	// CurrentTimeInms+=10;
}

void processvcount()
{
	if(!choosingfile) {
		frameCountForFrameSkip++;
		if (frameCountForFrameSkip > MAX_FRAMESKIP) {
			frameCountForFrameSkip = 0;
		}
	} else {
		frameCountForFrameSkip = 0;
	}
}

void processvblank()
{
	if(!choosingfile) {
		dsFrameCount++;
		// FPS = pdFrameCount;
		// FPS = ((float)pdFrameCount / (float)dsFrameCount)*60;
		// FPS = (60.0/(float)dsFrameCount)*pdFrameCount;
		if(dsFrameCount > 59) {
			FPS = pdFrameCount;
			pdFrameCount = dsFrameCount = 0;
		}
		else if((60 % dsFrameCount) == 0) {
			FPS = (60/dsFrameCount)*pdFrameCount;
		}
		//  EmulateFrame();
		dosVibrate();
	} else {
		if (currentWidth != width256) {
			if (scalemode != 1) ChangeScaleMode();
			currentWidth = width256;
		} else {
			dosVibrate();
		}
	}
}

/*
void EmulateSound() {
	PsndRate=WaveRate;
	PsndLen=WaveLen;
	PsndOut=WaveDest;

	// Clear the FIFO queue
	REG_IPC_FIFO_CR = IPC_FIFO_ENABLE | IPC_FIFO_SEND_CLEAR;
	// Send the FIFO msg
	REG_IPC_FIFO_TX = 1;

	DrawFrame();
	// Convert to signed:
	//  for (p=0;p<PsndLen<<1;p++) PsndOut[p]+=0x8000;

	PsndRate=PsndLen=0;
	PsndOut=NULL;
}
*/

void on_irq()
{
	if(REG_IF & IRQ_VBLANK) {
		// DrawFrame();
		if(choosingfile) {
			// if(choosingfile == 1)
			//
		}
		else {
			//iprintf("I AM ABOUT TO CALL EMULATEFRAME THIS IS SO EXCITING\n");
			// EmulateFrame();
			// EmulateSound();
			// DrawFrame();
		}
		// Tell the DS we handled the VBLANK interrupt
		INTR_WAIT_FLAGS |= IRQ_VBLANK;
		REG_IF |= IRQ_VBLANK;
	}
	else if(REG_IF & IRQ_TIMER0) {
		processtimer();
		INTR_WAIT_FLAGS |= IRQ_TIMER0;
	}
	else {
		// Ignore all other interrupts
		REG_IF = REG_IF;
	}
}

/*
void LidHandler() {
		if(IPC->buttons ==  0x00FF) {
			swiChangeSoundBias(0,0x400);
			powerOFF(POWER_ALL);
			// REG_IE=IRQ_PM;
		}
		else {
			swiChangeSoundBias(1,0x400);
			powerON(POWER_ALL);
		}
}
*/

void InitInterruptHandler()
{
	//irqInit();
	irqSet(IRQ_VCOUNT, processvcount);
	irqEnable(IRQ_VCOUNT);
	irqSet(IRQ_VBLANK, processvblank);
	irqEnable(IRQ_VBLANK);
	// irqSet(IRQ_LID, LidHandler);
	// irqEnable(IRQ_LID);
	// Setup a 100Hz = 10ms timer
	/*
	TIMER0_DATA = TIMER_FREQ_1024(100);
	TIMER0_CR = TIMER_ENABLE | TIMER_DIV_1024 | TIMER_IRQ_REQ;

	irqEnable(IRQ_TIMER0);
	irqSet(IRQ_TIMER0, processtimer);
	*/

	/*REG_IME = 0;
	IRQ_HANDLER = on_irq;
	REG_IE = IRQ_VBLANK | IRQ_TIMER0;
	REG_IF = ~0;
	DISP_SR = DISP_VBLANK_IRQ;
	REG_IME = 1;
	*/
}


int FileChoose()
{
	consoleClear();

	vector<string> extensionList;
	extensionList.emplace_back(".bin");
	extensionList.emplace_back(".gen");
	extensionList.emplace_back(".smd");

	std::string filename = browseForFile(extensionList);

	romfile=fopen(filename.c_str(), "rb");

	if(strcmp(filename.c_str(), "NULL") == 0 || romfile == NULL) {
		return 0; // we didn't get a file
	}
	else {
		sprintf(fileName, "%s", filename.c_str());
		return 1; // we got a file
	}
}

int EmulateExit()
{
	if(!UsingAppendedRom && FileChoose()) {
		// Save SRAM
		if(Pico.m.sram_changed) {
			saveLoadGame(0,1);
			Pico.m.sram_changed = 0;
		}

		// Remove cartridge
		PicoCartInsert(NULL,0);
		if (RomData && !UsingAppendedRom && !UsingExtendedMemory) {
			free(RomData); RomData=NULL; RomSize=0;
		}
		else if(RomData && UsingExtendedMemory) {
			RomData=NULL; RomSize=0;
		}

		PicoExit();
		return 1;
	}
	else {
		return 0;
	}
}

void LoadROMToMemory(uint16* location, int size)
{
	size=(size+3)&~3; // Round up to a multiple of 4
	
	fseek(romfile,0,SEEK_SET);
	fread(location,1,size,romfile);
	fclose(romfile);
	
	// Check for SMD:
	if ((size&0x3fff)==0x200) {
		DecodeSmd((unsigned char *)location,size); size-=0x200;
	} // Decode and byteswap SMD
	else Byteswap((unsigned char *)location,size); // Just byteswap

	RomData = (unsigned char *)location;
	RomSize = size;
}

int EmulateInit()
{
	int i;
	PicoInit();

	// romfile=fopen("/SONIC.BIN","rb");

	if(UsingAppendedRom) {
		PicoCartInsert(RomData,RomSize);
#ifdef ARM9_SOUND
		InitSound();
#endif
		// Load SRAM
		saveLoadGame(1,1);
	}
	else {	
		if(romfile != NULL) {
			fseek(romfile,0,SEEK_END);
			i = ftell(romfile);
			// iprintf("ftell: %i\n",i);
			if (isDSiMode()) {
				UsingExtendedMemory = true;
				LoadROMToMemory((uint16*)0x02800000,i);
			} else if(i >= 0x280000) {
				struct stat st;
				stat("/",&st);
				if((st.st_dev == DEVICE_TYPE_SCSD) || (st.st_dev == DEVICE_TYPE_SCCF)) { // cart is SCSD/SCCF
					iprintf("Using SuperCard RAM\n");

					UsingExtendedMemory = true;

					// Unlock SDRAM
					*SC_UNLOCK = 0xa55a;
					*SC_UNLOCK = 0xa55a;
					*SC_UNLOCK = 0x0007;
					*SC_UNLOCK = 0x0007;

					LoadROMToMemory(GBAROM,i);
				}
				else { // check for Opera RAM expansion
					OPERA_UNLOCK = 0x0001;
					DC_FlushAll();
					*OPERA_RAM = 0xF00D;
					if(*OPERA_RAM == 0xF00D) { // we successfully wrote into OPERA_RAM
						iprintf("Using Opera RAM Expansion\n");

						UsingExtendedMemory = true;

						LoadROMToMemory((uint16*)OPERA_RAM,i);
					}
				}
			}
			else {
				UsingExtendedMemory = false;
				PicoCartLoad(romfile,&RomData,&RomSize);
			}
			fclose(romfile);
			iprintf("Loaded.\n");

			PicoCartInsert(RomData,RomSize);
#ifdef ARM9_SOUND
			InitSound();
#endif
		}
		else {
			iprintf("Error opening file");
		}

		// Load SRAM
		saveLoadGame(1,1);
	}
	
	iprintf("ROM Size: %d\n",RomSize);
	iprintf("ROM Header Info:\n");
	for(i = 0; i < 128; i+=2) {
		if(!(RomData[0x100+i] == ' ' && RomData[0x100+i+1] == ' ' && RomData[0x100+i+2] == ' ')) {
			iprintf("%c",RomData[0x100+i+1]);
			iprintf("%c",RomData[0x100+i]);
		}
	}
	
	// iprintf("\n%#x\n",Pico.m.hardware);
	// PrintRegion();
	
	// iprintf("\x1b[10;0H");
	iprintf("\n\t\tPicoDriveDS ");
	// iprintf(VERSION_NO);
	iprintf("\n");

	cx = 32;
	cy = 16;
	if(scalemode == 1) {
		REG_BG3X = cx << 8;
		REG_BG3Y = cy << 8;
	} else {
		REG_BG3Y = (-6) << 8;
	}

	choosingfile = 0;

	return 0;
}

void FindAppendedRom(void)
{
	iprintf("Appended ROM check...");
	
	sysSetBusOwners(BUS_OWNER_ARM9,BUS_OWNER_ARM9);

	unsigned char *rom=NULL; int size=0;
	
	char *rompointer = (char *) GBAROM;
	char *genheader;
	bool foundrom = false;
	bool smdformat = false;

	while (!foundrom && (rompointer != ((char *)(GBAROM+0x02000000)))) {
		genheader = rompointer + 0x100; // Genesis ROM header is 0x100 in to the ROM
		// check for "SEGA " at genheader location (without including "SEGA " in our compiled binary)
		if( (*genheader == 'S') && (*(genheader+1) == 'E') && 
			(*(genheader+2) == 'G') && (*(genheader+3) == 'A') && 
			(*(genheader+4) == ' ') ) { // we have a match
			iprintf("FOUND ROM!\n");
			smdformat = false;
			foundrom = true;
		}
		// SMD format ROMs should have 0xaa and 0xbb @ 0x08 and 0x09
		else if( (*(rompointer+0x08) == 0xaa) && (*(rompointer+0x09) == 0xbb) ) { // check for SMD format ROM
			iprintf("FOUND SMD!\n");
			smdformat = true;
			foundrom = true;
		}
		else {
			rompointer += 256;
		}
	}
	if(foundrom) {
		if(smdformat) {
			// best guess is that size for SMD ROM is at 0x2d1
			genheader = rompointer + 0x2d1;
		}
		else {
			// End/size of normal ROM should be a uint at 0x1a4
			genheader = rompointer + 0x1a4;
		}
		
		size = (*(genheader) << 24) | (*(genheader+1) << 16) | (*(genheader+2) << 8) | (*(genheader+3));
		size=(size+3)&~3; // Round up to a multiple of 4
		if(smdformat) {
			size += 0x200; // add SMD header to size
		}
		
		// This if check is kind of pointless, how many SC users
		// are going to use appended ROMs if FAT works?
		// Might be useful for some other card where we can detect
		// insertion and use GBAROM but not FAT.
		if(0) {
			iprintf("Supercard detected.\n");
			rom=(unsigned char *)rompointer;
		}
		else {
			//Allocate space for the rom plus padding
			rom=(unsigned char *)malloc(size+4);

			memcpy(rom,rompointer,size);
		}
		
		// Check for SMD:
		if(smdformat) {
			DecodeSmd(rom,size); // decode and byteswap SMD
			size -= 0x200;
		}
		else {
			// Byteswap the ROM
			Byteswap(rom,size);
		}

		RomData = rom;
		RomSize = size;	
		
		UsingAppendedRom = true;
	}
	else {
		iprintf("ROM NOT FOUND!\n");
		RomData = NULL;
		UsingAppendedRom = false;
	}
}

int main(int argc, char **argv)
{
	extern void dsiOnly(void);
	dsiOnly();

	defaultExceptionHandler();

	bool fatInited = fatInitDefault();
	soundEnable();

	// ClearMemory();
	//resetMemory2_ARM9();
	//powerOn(POWER_ALL);

	//struct mallinfo mi;
	
	// videoSetMode(MODE_FB0);
	// vramSetBankA(VRAM_A_LCD);

	videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);

	// Set up the sub screen
	videoSetModeSub(MODE_5_2D | DISPLAY_BG0_ACTIVE);
	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankB(VRAM_B_MAIN_BG);
	vramSetBankC(VRAM_C_SUB_BG);
	vramSetBankD(VRAM_D_LCD);

	//vramSetMainBanks(VRAM_A_MAIN_BG, VRAM_B_MAIN_BG, VRAM_C_SUB_BG , VRAM_D_LCD);

#ifdef SW_FRAME_RENDERER
	REG_BG3CNT = BG_BMP16_512x256;
	PicoPrepareCram = UpdatePalette;
	PicoCramHigh = cram_high;
#endif
	
#ifdef SW_SCAN_RENDERER
	REG_BG3CNT = BG_BMP16_512x256;
	PicoCramHigh = cram_high;
#endif

#ifdef NDS_FRAME_RENDERER
	REG_BG0CNT = BG_64x64 | BG_TILE_BASE(4) | BG_MAP_BASE(0) | BG_COLOR_256;
	REG_BG3CNT = BgSize_R_512x512 | BG_TILE_BASE(4) | BG_MAP_BASE(0);
	
	uint16 val;
	for(val = 0; val < 256; val++) {
		(BG_PALETTE)[val] = RGB15(val%31,val%31,val%31);
	}
	
	for(val = 0; val < (64*64); val++) {
		((uint16*)BG_MAP_RAM(0))[val] = 0x0001; // 0x4000;
	}
	
	for(val = 0; val < 128; val++) {
		((uint16*)BG_TILE_RAM(4))[val] = 0x1e00;	
	}
#endif

	s16 c = cosLerp(0) >> 4;

	// s16 s = SIN[0] >> 4;
	// REG_BG3PA = ( c * (319))>>8;
	// REG_BG3PB = (-s * (320-8))>>8;
	// REG_BG3PC = ( s * (512-224+8))>>8;
	// REG_BG3PD = ( c * (300))>>8;

	/*
	BG3_XDX = ( c * (256))>>8;
	BG3_YDY = ( c * (256))>>8;
	BG3_CX = 32 << 8;
	BG3_CY = 16 << 8;
	*/

	REG_BG3X  = 0;
	REG_BG3Y  = 0;
	REG_BG3PA = ( c * (316))>>8;
	REG_BG3PD = ( c * (300))>>8;
	// REG_BG3Y = 6 << 8;


	/*
	BG3_XDX = 1 << 8;
	BG3_XDY = 0;
	BG3_YDX = 0;
	BG3_YDY = 1 << 8;
	*/

	// REG_BG0CNT_SUB = BG_MAP_BASE(31);

	// Set the colour of the font to White.
	//BG_PALETTE_SUB[255] = RGB15(31,31,31);

	// consoleInitDefault((u16*)SCREEN_BASE_BLOCK_SUB(31), (u16*)CHAR_BASE_BLOCK_SUB(0), 16);
	consoleDemoInit();

	bool nitroInited = nitroFSInit(argv[0]);
	if (!nitroInited) {
		iprintf("NitroFS init failed!\n");
		while(1);
	}

	CtrDspProcess* testProc = new CtrDspProcess();
	if(!testProc->ExecuteDsp1("nitro:/PicoDriveDS.cdc"))
	{
		iprintf("DSP Init Fail!\n");
		while(1);
	}
	iprintf("DSP Init OK!\n");

	// lcdSwap();

	// SoundInit();

	InitInterruptHandler();

	// iprintf("About to call InitFiles()...\n");


#ifdef ARM9_SOUND
	PsndRate = 11025;
#endif
	
	//iprintf("\nTrying to init FAT...\n");
	


	if(fatInited) {
		iprintf("\x1b[2J");

		if (argc > 1) {
			sprintf(fileName, "%s", argv[1]);
		} else if (!FileChoose()) {
			consoleClear();
			return 1;
		}
	}
	else {
		iprintf("FAT init failed.\n");
		FindAppendedRom();
		if(!UsingAppendedRom) {
			iprintf("\nNo way to load ROMs found.");
			return 1;
		}	
	}

	EmulateInit();

	/*
	videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);
	vramSetMainBanks(VRAM_A_MAIN_BG_0x6000000, VRAM_B_MAIN_BG_0x6020000, VRAM_C_MAIN_BG_0x6000000, VRAM_D_MAIN_BG_0x6020000);

	SUB_BG3_CR = BG_BMP16_512x256;

	SUB_BG3_XDX = ( c * (320))>>8;
	SUB_BG3_YDY = ( c * (320))>>8;
	SUB_BG3_CY  = 6 << 8;
	*/

	/*
	int offset;
	offset = (char *)&Pico.video - (char *)&Pico;
	iprintf("\nPico.video: %x\n",offset);
	offset = (char *)&Pico.vram - (char *)&Pico;
	iprintf("Pico.vram: %x\n",offset);
	offset = (char *)&Pico.vsram - (char *)&Pico;
	iprintf("Pico.vsram: %x\n",offset);
	offset = (char *)&Pico.cram - (char *)&Pico;
	iprintf("Pico.cram: %x\n",offset); 
	*/

	// u32 *testMem;
	// int mallCount = 0;

	while(1) {
		if(choosingfile) {
			ConvertToGrayscale();
			if(EmulateExit()) {
				EmulateInit();
			}
			else {
				consoleClear();
			}
			choosingfile = 0;
		}
		/*
		testMem = malloc(128);
		if(testMem != NULL) {
			mallCount++;
		}
		iprintf("\x1b[17;0HBytes/128: %d              ",mallCount);
		*/

		EmulateFrame();
		// Save SRAM
		if(Pico.m.sram_changed) {
			// iprintf("\x1b[17:0HSaving SRAM");
			saveLoadGame(0,1);
			Pico.m.sram_changed = 0;
		}
		// dmaCopy(BG_GFX,BG_GFX_SUB,512*256*2);
		// iprintf("\x1b[17;0H                        ");

		// iprintf("\x1b[17;0HFPS: %d   ",FPS);

		/*
		mi = mallinfo();
		iprintf("\n%i\t\tarena\n%i\t\tordblks\n%i\t\tuordblks\n%i\t\tfordblks\n",mi.arena,mi.ordblks,mi.uordblks,mi.fordblks);

		swiWaitForVBlank();
		// LastSecond = (IPC->curtime)[7];
		*/
	}
	return 0;
}
