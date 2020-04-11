#pragma once

#define DSP_PIPE_CMD_REG        2

#define DSP_PIPE_DIR_COUNT      2

#define DSP_PIPE_DIR_IN         0
#define DSP_PIPE_DIR_OUT        1

#define DSP_PIPE_PORT_COUNT     8

#define DSP_PIPE_PORT_CONSOLE   0
#define DSP_PIPE_PORT_DMA       1
#define DSP_PIPE_PORT_AUDIO     2
#define DSP_PIPE_PORT_BINARY    3
#define DSP_PIPE_PORT_TEMP      4

typedef void (*dsp_pipe_port_callback_t)(void* arg, int port, int dir);

#define DSP_PIPE_FLAG_DIR_MASK      1
#define DSP_PIPE_FLAG_PORT_MASK     0xFE
#define DSP_PIPE_FLAG_PORT_SHIFT    1
#define DSP_PIPE_FLAG_OPEN          (1 << 8)
#define DSP_PIPE_FLAG_EOF           (1 << 9)
#define DSP_PIPE_FLAG_EXIT          (1 << 15)

struct dsp_pipe_t
{
    u16 addr;
    u16 len;
    u16 readPtr;
    u16 writePtr;
    u16 flags;
};

struct dsp_pipe_mon_t
{
    dsp_pipe_t pipes[DSP_PIPE_PORT_COUNT][DSP_PIPE_DIR_COUNT];
};


void dsp_initPipe();
void dsp_pipeIrqCallback(void* arg);
void dsp_setPipePortCallback(int port, dsp_pipe_port_callback_t func, void* arg);
dsp_pipe_t* dsp_loadPipe(dsp_pipe_t* pipe, int port, int dir);
void dsp_syncPipe(dsp_pipe_t* pipe);
void dsp_flushPipe(dsp_pipe_t* pipe);
u16 dsp_getPipeReadLength(const dsp_pipe_t* pipe);
u16 dsp_getPipeWriteLength(const dsp_pipe_t* pipe);
void dsp_readPipe(dsp_pipe_t* pipe, void* dst, u16 length);
void dsp_writePipe(dsp_pipe_t* pipe, const void* src, u16 length);