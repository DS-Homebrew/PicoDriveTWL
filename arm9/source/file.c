#include <nds.h>
#include <stdio.h>
#include "pico/PicoInt.h"
#include "tonccpy.h"

char fileName[256];

void loadRomBank(int page, int i) {
	extern FILE* romfile;
	extern bool UsingExtendedMemory;

	if(!UsingExtendedMemory) return;

	if (page == 0) {
		tonccpy(Pico.rom+0x80000+(i*0x80000), Pico.rom, 0x80000);	// Read first-loaded bytes
		return;
	}

	romfile = fopen(fileName, "rb");

	u32 size = 0;
	fseek(romfile,0,SEEK_END);
	size = ftell(romfile);

	fseek(romfile, page*0x80000, SEEK_SET);
	fread(Pico.rom+0x80000+(i*0x80000), 1, 0x80000, romfile);
	fclose(romfile);

	// Check for SMD:
	if ((size&0x3fff)==0x200) {
		DecodeSmd(Pico.rom+0x80000+(i*0x80000),0x80000);
	} // Decode and byteswap SMD
	else Byteswap(Pico.rom+0x80000+(i*0x80000),0x80000); // Just byteswap

	/*iprintf("\x1b[14;0HFile   : %s",fileName);
	iprintf("\x1b[17;0HPage   : %d        ",page);
	iprintf("\x1b[18;0HROM src: %X        ",(int)page*0x80000);
	iprintf("\x1b[19;0HROM dst: %X        ",(int)0x80000+(i*0x80000));
	iprintf("\x1b[20;0HRAM dst: %X        ",(int)Pico.rom+0x80000+(i*0x80000));*/
}
