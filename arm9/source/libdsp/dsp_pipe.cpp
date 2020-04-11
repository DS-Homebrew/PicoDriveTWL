#include <nds.h>
#include <stdio.h>
#include "dsp.h"
#include "dsp_mem.h"
#include "dsp_fifo.h"
#include "dsp_pipe.h"

static volatile u32 sPipeMntrAddr = 0;
static dsp_pipe_port_callback_t sPipePortCallbackFuncs[DSP_PIPE_PORT_COUNT];
static void* sPipePortCallbackArgs[DSP_PIPE_PORT_COUNT];
static dsp_pipe_mon_t sDefaultPipes;

void dsp_initPipe()
{
    sPipeMntrAddr = 0;
    for(int i = 0; i < DSP_PIPE_PORT_COUNT; i++)
    {
        sPipePortCallbackFuncs[i] = NULL;
        sPipePortCallbackArgs[i] = NULL;
    }
}

void dsp_pipeIrqCallback(void* arg)
{
    //iprintf("dsp_pipeCallback!\n");
    while((dsp_getSemaphore() & 0x8000) || dsp_receiveDataReady(DSP_PIPE_CMD_REG))
    {
        dsp_clearSemaphore(0x8000);
        while(dsp_receiveDataReady(DSP_PIPE_CMD_REG))
        {
            if(!sPipeMntrAddr)
            {
                sPipeMntrAddr = DSP_MEM_ADDR_TO_CPU(dsp_receiveData(DSP_PIPE_CMD_REG));
                iprintf("sPipeMntrAddr = %x\n", sPipeMntrAddr);
            }
            else
            {
                u16 data = dsp_receiveData(DSP_PIPE_CMD_REG);
                //iprintf("Received %x\n", data);
                int port = data >> 1;
                int dir = data & 1;
                if(sPipePortCallbackFuncs[port])
                    sPipePortCallbackFuncs[port](sPipePortCallbackArgs[port], port, dir);
                else
                {
                    dsp_pipe_t pipe;
                    dsp_loadPipe(&pipe, port, dir);
                }                
            }            
        }
    }
}

void dsp_setPipePortCallback(int port, dsp_pipe_port_callback_t func, void* arg)
{
    sPipePortCallbackFuncs[port] = func;
    sPipePortCallbackArgs[port] = arg;
}

dsp_pipe_t* dsp_loadPipe(dsp_pipe_t* pipe, int port, int dir)
{
    if(port < 0 || port >= DSP_PIPE_PORT_COUNT || dir < 0 || dir >= DSP_PIPE_DIR_COUNT)
        return NULL;   
    while(!sPipeMntrAddr);
    if(!pipe)
        pipe = &sDefaultPipes.pipes[port][dir];
    dsp_fifoReadData(DSP_MEM_ADDR_TO_DSP(&((dsp_pipe_mon_t*)sPipeMntrAddr)->pipes[port][dir]), (u16*)pipe, DSP_MEM_ADDR_TO_DSP(sizeof(dsp_pipe_t)));
    return pipe;
}

void dsp_syncPipe(dsp_pipe_t* pipe)
{
    dsp_loadPipe(pipe, (pipe->flags & DSP_PIPE_FLAG_PORT_MASK) >> DSP_PIPE_FLAG_PORT_SHIFT, pipe->flags & DSP_PIPE_FLAG_DIR_MASK);
}

void dsp_flushPipe(dsp_pipe_t* pipe)
{
    int port = (pipe->flags & DSP_PIPE_FLAG_PORT_MASK) >> DSP_PIPE_FLAG_PORT_SHIFT;
    int dir = pipe->flags & DSP_PIPE_FLAG_DIR_MASK;
    dsp_pipe_t* dspPipe = &((dsp_pipe_mon_t*)sPipeMntrAddr)->pipes[port][dir];
    if(dir == DSP_PIPE_DIR_IN)
        dsp_fifoWriteData(&pipe->readPtr, DSP_MEM_ADDR_TO_DSP(&dspPipe->readPtr), 1);
    else
        dsp_fifoWriteData(&pipe->writePtr, DSP_MEM_ADDR_TO_DSP(&dspPipe->writePtr), 1);
    swiDelay(8);
    while(!(REG_DSP_PSTS & DSP_PSTS_WR_FIFO_EMPTY) || (REG_DSP_PSTS & DSP_PSTS_WR_XFER_BUSY));
    dsp_sendData(DSP_PIPE_CMD_REG, pipe->flags & (DSP_PIPE_FLAG_PORT_MASK | DSP_PIPE_FLAG_DIR_MASK));
}

u16 dsp_getPipeReadLength(const dsp_pipe_t* pipe)
{
    return ((pipe->writePtr - pipe->readPtr) + (((pipe->readPtr ^ pipe->writePtr) < 0x8000) ? 0 : pipe->len)) & ~0x8000;
}

u16 dsp_getPipeWriteLength(const dsp_pipe_t* pipe)
{
    return ((pipe->readPtr - pipe->writePtr) + (((pipe->writePtr ^ pipe->readPtr) < 0x8000) ? 0 : pipe->len)) & ~0x8000;
}

void dsp_readPipe(dsp_pipe_t* pipe, void* dst, u16 length)
{
    int irqs = enterCriticalSection();
    {
        u8* pDst = (u8*)dst;
        bool changed = false;
        dsp_syncPipe(pipe);
        while(length > 0)
        {
            u16 phase = pipe->readPtr ^ pipe->writePtr;
            if(phase)
            {
                u16 curPos = pipe->readPtr & ~0x8000;
                u16 endPos = (phase < 0x8000) ? pipe->writePtr & ~0x8000 : pipe->len;
                u16 curLen = endPos - curPos;
                if(length < curLen)
                    curLen = length;
                curLen &= ~1;
                dsp_fifoReadData(pipe->addr + DSP_MEM_ADDR_TO_DSP(curPos), (u16*)pDst, DSP_MEM_ADDR_TO_DSP(curLen));
                length -= curLen;
                pDst += curLen;
                pipe->readPtr = (curPos + curLen < pipe->len) ? pipe->readPtr + curLen : (~pipe->readPtr & 0x8000);
                changed = true;
            }
            else
            {
                if(changed)
                {
                    dsp_flushPipe(pipe);
                    changed = false;
                }
                iprintf("Sync!\n");
                //syncing again here?? Or should I wait for an irq or so, but irqs are disabled??
                dsp_syncPipe(pipe);
            }            
        }
        if(changed)
            dsp_flushPipe(pipe);
    }
    leaveCriticalSection(irqs);
}

void dsp_writePipe(dsp_pipe_t* pipe, const void* src, u16 length)
{
    int irqs = enterCriticalSection();
    {
        const u8* pSrc = (const u8*)src;
        bool changed = false;
        dsp_syncPipe(pipe);
        while(length > 0)
        {
            u16 phase = pipe->readPtr ^ pipe->writePtr;
            if(phase != 0x8000)
            {
                u16 curPos = pipe->writePtr & ~0x8000;
                u16 endPos = (phase < 0x8000) ? pipe->len : (pipe->readPtr & ~0x8000);
                u16 curLen = endPos - curPos;
                if(length < curLen)
                    curLen = length;
                curLen &= ~1;
                dsp_fifoWriteData((u16*)pSrc, pipe->addr + DSP_MEM_ADDR_TO_DSP(curPos), DSP_MEM_ADDR_TO_DSP(curLen));
                length -= curLen;
                pSrc += curLen;
                pipe->writePtr = (curPos + curLen < pipe->len) ? pipe->writePtr + curLen : (~pipe->writePtr & 0x8000);
                changed = true;
            }
            else
            {
                if(changed)
                {
                    dsp_flushPipe(pipe);
                    changed = false;
                }
                iprintf("Sync!\n");
                //syncing again here?? Or should I wait for an irq or so, but irqs are disabled??
                dsp_syncPipe(pipe);
            }            
        }
        if(changed)
            dsp_flushPipe(pipe);
    }
    leaveCriticalSection(irqs);
}