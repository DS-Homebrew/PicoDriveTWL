#include <nds.h>
#include "dsp.h"
#include "dsp_pipe.h"
#include "CmnDspProcess.h"

static void audioCallback(void* arg, int port, int dir)
{
	if(dir != DSP_PIPE_DIR_IN)
        return;
    dsp_pipe_t pipe;
	dsp_loadPipe(&pipe, port, dir);
    if(dsp_getPipeReadLength(&pipe) < sizeof(dsp_audio_driver_resp_t))
        return;
    dsp_audio_driver_resp_t resp;
    dsp_readPipe(&pipe, &resp, sizeof(dsp_audio_driver_resp_t));
    resp.ctrl = DSP_MEM_32BIT_TO_DSP(resp.ctrl);
    resp.result = DSP_MEM_32BIT_TO_DSP(resp.result);
}

void CmnDspProcess::SendAudioDriverCommand(DspAudioDriverTarget target, DspAudioDriverControl control, DspAudioDriverMode mode, void* buffer, u32 length, u32 option)
{
    dsp_setPipePortCallback(DSP_PIPE_PORT_AUDIO, audioCallback, NULL);
    dsp_audio_driver_cmd_t cmd;
	cmd.ctrl = DSP_MEM_32BIT_TO_DSP(DSP_AUDIO_DRIVER_TARGET(target) | DSP_AUDIO_DRIVER_CONTROL(control) | DSP_AUDIO_DRIVER_MODE(mode));
    cmd.buf = DSP_MEM_32BIT_TO_DSP(buffer);
    cmd.len = DSP_MEM_32BIT_TO_DSP(length);
    cmd.opt = DSP_MEM_32BIT_TO_DSP(option);
    dsp_pipe_t pipe;
	dsp_loadPipe(&pipe, DSP_PIPE_PORT_AUDIO, DSP_PIPE_DIR_OUT);
    dsp_writePipe(&pipe, &cmd, sizeof(cmd));
}

void CmnDspProcess::PlaySound(const void* buffer, u32 length, bool stereo)
{
    SendAudioDriverCommand(DSP_AUDIO_DRIVER_TARGET_OUTPUT, DSP_AUDIO_DRIVER_CONTROL_START, stereo ? DSP_AUDIO_DRIVER_MODE_STEREO : DSP_AUDIO_DRIVER_MODE_MONO, (void*)buffer, length >> 1, 0);
}

void CmnDspProcess::StopSound()
{
    SendAudioDriverCommand(DSP_AUDIO_DRIVER_TARGET_OUTPUT, DSP_AUDIO_DRIVER_CONTROL_START, DSP_AUDIO_DRIVER_MODE_MONO, NULL, 0, 0);
    //this command doesn't work for some reason
    //SendAudioDriverCommand(DSP_AUDIO_DRIVER_TARGET_OUTPUT, DSP_AUDIO_DRIVER_CONTROL_STOP, (DspAudioDriverMode)0, NULL, 0, 0);
}