#include <nds.h>
#include "dsp.h"
#include "dsp_pipe.h"
#include "CtrDspProcess.h"

void CtrDspProcess::OnPipeResponse(int port, int dir)
{
	if(dir != DSP_PIPE_DIR_IN)
        return;
    dsp_pipe_t pipe;
	dsp_loadPipe(&pipe, port, dir);
    if(dsp_getPipeReadLength(&pipe) < sizeof(dsp_codec_cmd_t))
         return;

	dsp_readPipe(&pipe, (void*)&_lastResponse, sizeof(dsp_codec_cmd_t));
    
    if(_lastResponse.codecType == DSP_CODEC_TYPE_AACDEC && _lastResponse.command == DSP_CODEC_CMD_PROCESS)
    {
        memcpy(&_lastAACDecResponse, &_lastResponse.params.aacDecProcResp, sizeof(dsp_codec_cmd_aacdec_proc_resp_t));
        _lastAACDecResponseValid = true;
    }

    _gotResponse = true;
}

bool CtrDspProcess::AACDecInitialize(
    DspCodecAACDecFormat      format, 
    DspCodecAACDecFrequency   frequency,
    DspCodecAACDecCRCFlag     crcFlag,
    DspCodecAACDecReverseFlag reverseFlag)
{
	dsp_setPipePortCallback(DSP_PIPE_PORT_BINARY, CtrDspProcess::OnPipeResponse, this);

    dsp_pipe_t pipe;
	dsp_loadPipe(&pipe, DSP_PIPE_PORT_BINARY, DSP_PIPE_DIR_OUT);

    dsp_codec_cmd_t cmd;
    cmd.codecType = DSP_CODEC_TYPE_AACDEC;
	cmd.command = DSP_CODEC_CMD_INITIALIZE;
	cmd.result = DSP_CODEC_RESULT_SUCCESS;
	cmd.platform = 0;
    cmd.params.aacDecInit.format = format;
    cmd.params.aacDecInit.frequency = frequency;
    cmd.params.aacDecInit.crcFlag = crcFlag;
    cmd.params.aacDecInit.reverseFlag = reverseFlag;
    cmd.params.aacDecInit.reserved = 0;

    _gotResponse = false;
    _lastAACDecResponseValid = false;

    dsp_writePipe(&pipe, &cmd, sizeof(cmd));

    WaitCodecResponse();

    return _lastResponse.result == DSP_CODEC_RESULT_SUCCESS;
}

void CtrDspProcess::AACDecDecode(const void* aacFrame, u32 frameLength, s16* leftOut, s16* rightOut)
{
    dsp_setPipePortCallback(DSP_PIPE_PORT_BINARY, CtrDspProcess::OnPipeResponse, this);

    dsp_pipe_t pipe;
	dsp_loadPipe(&pipe, DSP_PIPE_PORT_BINARY, DSP_PIPE_DIR_OUT);

    dsp_codec_cmd_t cmd;
    cmd.codecType = DSP_CODEC_TYPE_AACDEC;
	cmd.command = DSP_CODEC_CMD_PROCESS;
	cmd.result = DSP_CODEC_RESULT_SUCCESS;
	cmd.platform = 0;
    cmd.params.aacDecProc.dataAddress = (u32)aacFrame;
    cmd.params.aacDecProc.frameSize = frameLength;
    cmd.params.aacDecProc.leftPCMAddress = (u32)leftOut;
    cmd.params.aacDecProc.rightPCMAddress = (u32)rightOut;
    cmd.params.aacDecProc.paramAddress = 0;

    _gotResponse = false;

    dsp_writePipe(&pipe, &cmd, sizeof(cmd));
}