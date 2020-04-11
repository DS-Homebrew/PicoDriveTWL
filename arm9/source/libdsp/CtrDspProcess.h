#pragma once

#include "DspProcess.h"

enum DspCodecType : u16
{
    DSP_CODEC_TYPE_UNKNOWN = 0,
    DSP_CODEC_TYPE_AACDEC  = 1,
    DSP_CODEC_TYPE_AACENC  = 2,
    DSP_CODEC_TYPE_MP3DEC  = 3,
    DSP_CODEC_TYPE_MP3ENC  = 4
};

enum DspCodecCmd : u16
{
    DSP_CODEC_CMD_INITIALIZE = 0,
    DSP_CODEC_CMD_PROCESS    = 1,
    DSP_CODEC_CMD_FINALIZE   = 2,
    DSP_CODEC_CMD_RESUME     = 3,
    DSP_CODEC_CMD_SUSPEND    = 4
};

enum DspCodecResult : u16
{
    DSP_CODEC_RESULT_SUCCESS = 0,
    DSP_CODEC_RESULT_FAILURE = 1
};

enum DspCodecAACDecFormat : u32
{
    DSP_CODEC_AACDEC_FORMAT_RAW  = 0,
    DSP_CODEC_AACDEC_FORMAT_ADTS = 1
};

enum DspCodecAACDecFrequency : int
{
    DSP_CODEC_AACDEC_FREQUENCY_UNKNOWN = -1,
    DSP_CODEC_AACDEC_FREQUENCY_48000   = 0,
    DSP_CODEC_AACDEC_FREQUENCY_44100   = 1,
    DSP_CODEC_AACDEC_FREQUENCY_32000   = 2,
    DSP_CODEC_AACDEC_FREQUENCY_24000   = 3,
    DSP_CODEC_AACDEC_FREQUENCY_22050   = 4,
    DSP_CODEC_AACDEC_FREQUENCY_16000   = 5,
    DSP_CODEC_AACDEC_FREQUENCY_12000   = 6,
    DSP_CODEC_AACDEC_FREQUENCY_11025   = 7,
    DSP_CODEC_AACDEC_FREQUENCY_8000    = 8
};

enum DspCodecAACDecCRCFlag : u32
{
    DSP_CODEC_AACDEC_CRC_FLAG_OFF = 0,
    DSP_CODEC_AACDEC_CRC_FLAG_ON  = 1
};

enum DspCodecAACDecReverseFlag : u32
{
    DSP_CODEC_AACDEC_REVERSE_FLAG_OFF = 0,
    DSP_CODEC_AACDEC_REVERSE_FLAG_ON  = 1
};

struct dsp_codec_cmd_aacdec_init_t
{
    DspCodecAACDecFormat      format;
    DspCodecAACDecFrequency   frequency;
    DspCodecAACDecCRCFlag     crcFlag;
    DspCodecAACDecReverseFlag reverseFlag;
    u32 reserved;
};

struct dsp_codec_cmd_aacdec_proc_t
{
    u32 dataAddress;
    u32 frameSize;
    u32 leftPCMAddress;
    u32 rightPCMAddress;
    u32 paramAddress; //? can be kept 0
};

struct dsp_codec_cmd_aacdec_proc_resp_t
{
    DspCodecAACDecFrequency frequency;
    u32 nrChannels;
    u32 usedBytes;
    u32 errorCode;
    u32 nrClockCycles;
};

struct dsp_codec_cmd_t
{
    DspCodecType   codecType;
    DspCodecCmd    command;
    DspCodecResult result;
    u16 platform; //?
    union
    {
        dsp_codec_cmd_aacdec_init_t aacDecInit;
        dsp_codec_cmd_aacdec_proc_t aacDecProc;
        dsp_codec_cmd_aacdec_proc_resp_t aacDecProcResp;
        u16 raw[12];
    } params;    
};

class CtrDspProcess : public DspProcess
{
    void OnPipeResponse(int port, int dir);
    static void OnPipeResponse(void* arg, int port, int dir)
    {
        return ((CtrDspProcess*)arg)->OnPipeResponse(port, dir);
    }

protected:
    volatile bool _gotResponse;
    dsp_codec_cmd_t _lastResponse;
    dsp_codec_cmd_aacdec_proc_resp_t _lastAACDecResponse;
    bool _lastAACDecResponseValid;

public:
    CtrDspProcess()
        : _gotResponse(false), _lastAACDecResponseValid(false)
    {}

    bool AACDecInitialize(
        DspCodecAACDecFormat      format, 
        DspCodecAACDecFrequency   frequency   = DSP_CODEC_AACDEC_FREQUENCY_UNKNOWN,
        DspCodecAACDecCRCFlag     crcFlag     = DSP_CODEC_AACDEC_CRC_FLAG_ON,
        DspCodecAACDecReverseFlag reverseFlag = DSP_CODEC_AACDEC_REVERSE_FLAG_OFF);
    
    void AACDecDecode(const void* aacFrame, u32 frameLength, s16* leftOut, s16* rightOut);
    
    void WaitCodecResponse()
    {
        while(!_gotResponse);
    }

    int AACDecGetSampleRate()
    {
        static const int aacFrequencies[] =
            { 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000 };
        if(!_lastAACDecResponseValid || _lastAACDecResponse.errorCode != 0 || _lastAACDecResponse.frequency == DSP_CODEC_AACDEC_FREQUENCY_UNKNOWN)
            return -1;
        return aacFrequencies[(int)_lastAACDecResponse.frequency];
    }

    int AACDecGetChannelCount()
    {
        if(!_lastAACDecResponseValid || _lastAACDecResponse.errorCode != 0)
            return -1;
        return _lastAACDecResponse.nrChannels;
    }

    int AACDecGetErrorCode()
    {
        if(!_lastAACDecResponseValid)
            return -1;
        return _lastAACDecResponse.errorCode;
    }

    int AACDecGetUsedBytes()
    {
        if(!_lastAACDecResponseValid)
            return -1;
        return _lastAACDecResponse.usedBytes;
    }
};