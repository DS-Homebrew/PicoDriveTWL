#include "teak/teak.h"
#include "ipc.h"

#define SW_FRAME_RENDERER

#define BG_GFX			((u16*)0x6000000)		/**< \brief background graphics memory*/

extern void PicoFrameSimple();

int main()
{
    icu_init();
    dma_init();

    //*(vu16*)0x0600 = 0;

    //while(*(vu16*)0x80E0);

    ipc_init();
    
    while(1)
    {
		if (apbp_receiveData(0) == 0x4452) {
			PicoFrameSimple();
			//u16 color = 0x3FFF;
			//dma_transferDspToArm9((const void*)&color, BG_GFX, 1);
			apbp_sendData(0, 0);
		}
    }
    return 0;
}