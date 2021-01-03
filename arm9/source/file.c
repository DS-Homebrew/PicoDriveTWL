#include <nds.h>
#include <stdio.h>
#include "pico/PicoInt.h"
#include "tonccpy.h"

#define cacheAmount 4

extern char* romSpace;

char fileName[256];

static int cachedPages[cacheAmount] = {0};
static int currentPage = 0;

void initCachedPages(void) {
	for (int i = 0; i < cacheAmount; i++) {
		cachedPages[i] = 0;
	}
}

void loadRomBank(int page, int i) {
	extern FILE* romfile;
	extern bool UsingExtendedMemory;

	if(!UsingExtendedMemory) return;

	if (page == 0) {
		// Read first-loaded bytes
		if (isDSiMode()) {
			DC_FlushAll();
			dmaCopyWords(0, Pico.rom, Pico.rom+0x80000+(i*0x80000), 0x80000);
		} else {
			tonccpy(Pico.rom+0x80000+(i*0x80000), Pico.rom, 0x80000);
		}
		return;
	}

	for (int i2 = 0; i2 < cacheAmount; i2++) {
		if (cachedPages[i2] == page) {
			if (isDSiMode()) {
				DC_FlushAll();
				dmaCopyWords(0, (char*)romSpace+(i2*0x80000), Pico.rom+0x80000+(i*0x80000), 0x80000);
			} else {
				tonccpy(Pico.rom+0x80000+(i*0x80000), (char*)romSpace+(i2*0x80000), 0x80000);
			}
			return;
		}
	}

	char* bankCache = (char*)romSpace+(currentPage*0x80000);

	romfile = fopen(fileName, "rb");

	u32 size = 0;
	fseek(romfile,0,SEEK_END);
	size = ftell(romfile);

	fseek(romfile, page*0x80000, SEEK_SET);
	fread(bankCache, 1, 0x80000, romfile);
	fclose(romfile);

	// Check for SMD:
	if ((size&0x3fff)==0x200) {
		DecodeSmd((unsigned char*)bankCache,0x80000);
	} // Decode and byteswap SMD
	else Byteswap((unsigned char*)bankCache,0x80000); // Just byteswap

	if (isDSiMode()) {
		DC_FlushAll();
		dmaCopyWords(0, bankCache, Pico.rom+0x80000+(i*0x80000), 0x80000);
	} else {
		tonccpy(Pico.rom+0x80000+(i*0x80000), bankCache, 0x80000);
	}
	cachedPages[currentPage] = page;
	currentPage++;
	if (currentPage >= cacheAmount) currentPage = 0;

	/*iprintf("\x1b[14;0HFile   : %s",fileName);
	iprintf("\x1b[17;0HPage   : %d        ",page);
	iprintf("\x1b[18;0HROM src: %X        ",(int)page*0x80000);
	iprintf("\x1b[19;0HROM dst: %X        ",(int)0x80000+(i*0x80000));
	iprintf("\x1b[20;0HRAM dst: %X        ",(int)Pico.rom+0x80000+(i*0x80000));*/
}
