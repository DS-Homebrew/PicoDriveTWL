#include "teak/teak.h"
#include "ipc.h"

int main()
{
    icu_init();
    dma_init();

    *(vu16*)0x0600 = 0;

    while(*(vu16*)0x80E0);

    ipc_init();
    
    //u32 addr = 0x02022F00;
    //int val = 0;
    while(1)
    {
    }

    // u16 val = 0;
    // while(1)
    // {
    //     dma_transferArm9ToDsp(0x04000130, (void*)0x0601, 1);
    //     if(((*(vu16*)0x0601) & 1) == 0)
    //         setColor(val++);
    //     for(int i = 166666; i >= 0; i--)
    //         asm("nop");
    // }

    // while(1);

    // // while(((*(vu16*)0x818C) & 0x80) == 0);
    // // u16 color = 0;
    // // while(1)
    // // {
    // //     *(vu16*)0x0600 = color;
    // //     dma_transferDspToArm9((const void*)0x0600, 0x05000000, 1);
    // //     // for(int i = 1666666; i >= 0; i--)
    // //     // {
    // //     //     asm("nop");
    // //     // }
    // //     color++;
    // // }
    // //*(vu16*)0x0600 = 0x1F;
    // setColor(0x1F);
    // setColor(0x3F);
    // while(1);
    // //setColor(0x3E0);
    // *(vu16*)0x0600 = 123;
    // while(1);
    return 0;
}