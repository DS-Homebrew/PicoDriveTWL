#pragma once

#include "DspProcess.h"

enum DspAudioDriverTarget
{
    DSP_AUDIO_DRIVER_TARGET_OUTPUT = 1,  
    DSP_AUDIO_DRIVER_TARGET_INPUT  = 2,
    DSP_AUDIO_DRIVER_TARGET_CACHE  = 3  
};

#define DSP_AUDIO_DRIVER_TARGET_MASK    0xF000
#define DSP_AUDIO_DRIVER_TARGET_SHIFT   12
#define DSP_AUDIO_DRIVER_TARGET(x)      ((x) << DSP_AUDIO_DRIVER_TARGET_SHIFT)

enum DspAudioDriverControl
{
    DSP_AUDIO_DRIVER_CONTROL_START = 1,  
    DSP_AUDIO_DRIVER_CONTROL_STOP  = 2,
    DSP_AUDIO_DRIVER_CONTROL_LOAD  = 3,
    DSP_AUDIO_DRIVER_CONTROL_STORE = 4
};

#define DSP_AUDIO_DRIVER_CONTROL_MASK    0xF00
#define DSP_AUDIO_DRIVER_CONTROL_SHIFT   8
#define DSP_AUDIO_DRIVER_CONTROL(x)      ((x) << DSP_AUDIO_DRIVER_CONTROL_SHIFT)

enum DspAudioDriverMode
{
    DSP_AUDIO_DRIVER_MODE_MONO    = 0,  
    DSP_AUDIO_DRIVER_MODE_STEREO  = 1,
    DSP_AUDIO_DRIVER_MODE_HALFVOL = 2
};

#define DSP_AUDIO_DRIVER_MODE_MASK      0xFF
#define DSP_AUDIO_DRIVER_MODE_SHIFT     0
#define DSP_AUDIO_DRIVER_MODE(x)        ((x) << DSP_AUDIO_DRIVER_MODE_SHIFT)

struct dsp_audio_driver_cmd_t
{
	u32 ctrl;
	u32 buf;
	u32 len;
	u32 opt;
};

struct dsp_audio_driver_resp_t
{
    u32 ctrl;
    u32 result;
};

class CmnDspProcess : public DspProcess
{
protected:
    void SendAudioDriverCommand(DspAudioDriverTarget target, DspAudioDriverControl control, DspAudioDriverMode mode, void* buffer, u32 length, u32 option);

public:
    void PlaySound(const void* buffer, u32 length, bool stereo);
    void StopSound();
};