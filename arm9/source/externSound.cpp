#include "externSound.h"

#include "Pico/PicoInt.h"
#include "streamingaudio.h"
#include "string.h"
#include "tonccpy.h"
#include <algorithm>

//#define MSL_NSONGS		0
//#define MSL_NSAMPS		7
//#define MSL_BANKSIZE	7


extern volatile s16 fade_counter;
extern volatile bool fade_out;

extern volatile s16* play_stream_buf;
extern volatile s16* fill_stream_buf;

// Number of samples filled into the fill buffer so far.
extern volatile s32 filled_samples;

extern volatile bool fill_requested;
extern volatile s32 samples_left_until_next_fill;
extern volatile s32 streaming_buf_ptr;

#define SAMPLES_USED (STREAMING_BUF_LENGTH - samples_left)
#define REFILL_THRESHOLD STREAMING_BUF_LENGTH >> 2

#ifdef SOUND_DEBUG
extern char debug_buf[256];
#endif

extern volatile u32 sample_delay_count;

static bool loopingPoint = false;
static bool loopingPointFound = false;
static bool streamFound = false;

SoundControl::SoundControl()
	: stream_is_playing(false), stream_source(NULL), startup_sample_length(0)
 {

	/*sys.mod_count = MSL_NSONGS;
	sys.samp_count = MSL_NSAMPS;
	sys.mem_bank = SOUNDBANK;
	sys.fifo_channel = FIFO_MAXMOD;

	mmInit(&sys);*/

	loopingPoint = false;
	loopingPointFound = false;
	streamFound = false;
}

void SoundControl::loadStream(const char* filenameStart, const char* filename) {
	if (stream_source) {
		streamFound = false;
		fclose(stream_start_source);
		fclose(stream_source);
	}

	loopingPoint = false;
	loopingPointFound = false;

	if (strcmp(filenameStart, "") != 0) {
		stream_start_source = fopen(filenameStart, "rb");
		if (stream_start_source) {
			loopingPointFound = true;
		} else {
			loopingPoint = true;
		}
	}

	stream_source = fopen(filename, "rb");
	if (!stream_source) return;

	resetStreamSettings();

	stream.sampling_rate = 32000;	 		// 32000Hz
	stream.buffer_length = 800;	  			// should be adequate
	stream.callback = on_stream_request;    
	stream.format = MM_STREAM_16BIT_MONO;  // select format
	stream.timer = MM_TIMER0;	    	   // use timer0
	stream.manual = false;	      		   // auto filling
	streamFound = true;
	
	// Prep the first section of the stream
	fread((void*)play_stream_buf, sizeof(s16), STREAMING_BUF_LENGTH, loopingPointFound ? stream_start_source : stream_source);

	// Fill the next section premptively
	fread((void*)fill_stream_buf, sizeof(s16), STREAMING_BUF_LENGTH, loopingPointFound ? stream_start_source : stream_source);

	streamFound = true;
}

void SoundControl::beginStream() {
	if (!streamFound || stream_is_playing) return;

	// open the stream
	stream_is_playing = true;
	mmStreamOpen(&stream);
	SetYtrigger(0);
}

void SoundControl::stopStream() {
	if (!streamFound || !stream_is_playing) return;

	stream_is_playing = false;
	mmStreamClose();
}

void SoundControl::closeStream() {
	if (!streamFound || !stream_is_playing) return;

	streamFound = false;
	stream_is_playing = false;
	mmStreamClose();
}

void SoundControl::resetStream() {
	if (!streamFound) return;

	resetStreamSettings();

	fseek(stream_start_source, 0, SEEK_SET);
	fseek(stream_source, 0, SEEK_SET);

	// Prep the first section of the stream
	fread((void*)play_stream_buf, sizeof(s16), STREAMING_BUF_LENGTH, loopingPointFound ? stream_start_source : stream_source);

	// Fill the next section premptively
	fread((void*)fill_stream_buf, sizeof(s16), STREAMING_BUF_LENGTH, loopingPointFound ? stream_start_source : stream_source);
}

void SoundControl::fadeOutStream() {
	fade_out = true;
}

void SoundControl::cancelFadeOutStream() {
	fade_out = false;
	fade_counter = FADE_STEPS;
}

void SoundControl::setStreamDelay(u32 delay) {
	sample_delay_count = delay;
}


// Samples remaining in the fill buffer.
#define SAMPLES_LEFT_TO_FILL (abs(STREAMING_BUF_LENGTH - filled_samples))

// Samples that were already streamed and need to be refilled into the buffer.
#define SAMPLES_TO_FILL (abs(streaming_buf_ptr - filled_samples))

// Updates the background music fill buffer
// Fill the amount of samples that were used up between the
// last fill request and this.

// Precondition Invariants:
// filled_samples <= STREAMING_BUF_LENGTH
// filled_samples <= streaming_buf_ptr

// Postcondition Invariants:
// filled_samples <= STREAMING_BUF_LENGTH
// filled_samples <= streaming_buf_ptr
// fill_requested == false
volatile void SoundControl::updateStream() {
	
	if (!stream_is_playing) return;
	if (fill_requested && filled_samples < STREAMING_BUF_LENGTH) {
			
		// Reset the fill request
		fill_requested = false;
		int instance_filled = 0;

		// Either fill the max amount, or fill up the buffer as much as possible.
		int instance_to_fill = std::min(SAMPLES_LEFT_TO_FILL, SAMPLES_TO_FILL);

		// If we don't read enough samples, loop from the beginning of the file.
		instance_filled = fread((s16*)fill_stream_buf + filled_samples, sizeof(s16), instance_to_fill, loopingPoint ? stream_source : stream_start_source);
		if (instance_filled < instance_to_fill) {
			fseek(stream_source, 0, SEEK_SET);
			instance_filled += fread((s16*)fill_stream_buf + filled_samples + instance_filled,
				 sizeof(s16), (instance_to_fill - instance_filled), stream_source);
			loopingPoint = true;
		}

		#ifdef SOUND_DEBUG
		sprintf(debug_buf, "FC: SAMPLES_LEFT_TO_FILL: %li, SAMPLES_TO_FILL: %li, instance_filled: %i, filled_samples %li, to_fill: %i", SAMPLES_LEFT_TO_FILL, SAMPLES_TO_FILL, instance_filled, filled_samples, instance_to_fill);
		nocashMessage(debug_buf);
		#endif

		// maintain invariant 0 < filled_samples <= STREAMING_BUF_LENGTH
		filled_samples = std::min<s32>(filled_samples + instance_filled, STREAMING_BUF_LENGTH);
	

	} else if (fill_requested && filled_samples >= STREAMING_BUF_LENGTH) {
		// filled_samples == STREAMING_BUF_LENGTH is the only possible case
		// but we'll keep it at gte to be safe.
		filled_samples = 0;
		// fill_count = 0;
	}

}

extern bool playSound;
extern bool soundPaused;
extern mm_sfxhand sndHandlers[48];
static u8 currentMusId = 0;
static u8 prevSndId = 0;

extern u8 musFirstID;
extern u8 musLastID;

extern u8 sndFirstID;
extern u8 sndLastID;
extern u8 sndStopID;

extern u8 pauseID;
extern u8 unpauseID;

extern u16 snd68000addr[2];
extern u16 sndZ80addr[2];

extern u16 pause68000addr;
extern u16 pauseZ80addr;

extern char sndFilePath[3][256];
static char musFilePath[2][256] = {0};

bool MusicPlayRAM(void) {
	if (!playSound || snd68000addr[0]==0) return false;

	u8 soundId = 0;
	if (Pico.ram[snd68000addr[0]] >= musFirstID && Pico.ram[snd68000addr[0]] <= musLastID) {
		soundId = Pico.ram[snd68000addr[0]];
	} else if (Pico.ram[snd68000addr[1]] >= musFirstID && Pico.ram[snd68000addr[1]] <= musLastID) {
		soundId = Pico.ram[snd68000addr[1]];
	}
	if (soundId==0 || currentMusId==soundId) {
		return false;
	}
	snd().stopStream();
	currentMusId = soundId;

	//printf ("\x1b[22;1H");
	//printf ("Now Loading...");

	// External sound
	snprintf(musFilePath[0], 256, "%s%X_start.raw", sndFilePath[2], soundId);
	snprintf(musFilePath[1], 256, "%s%X.raw", sndFilePath[2], soundId);
	snd().loadStream(musFilePath[0], musFilePath[1]);

	//printf ("\x1b[22;1H");
	//printf ("              ");

	snd().beginStream();

	return true;
}

void SoundPlayRAM(void) {
	if (!playSound || snd68000addr[0]==0) return;

	if (pause68000addr != 0) {
		if (Pico.ram[pause68000addr] == pauseID && !soundPaused) {
			snd().stopStream();
			mmEffectCancelAll();
			soundPaused = true;
			return;
		} else if (Pico.ram[pause68000addr] == unpauseID && soundPaused) {
			snd().beginStream();
			soundPaused = false;
			return;
		}
	}

	if (Pico.ram[snd68000addr[0]] == sndStopID || Pico.ram[snd68000addr[1]] == sndStopID) {
		snd().closeStream();
		mmEffectCancelAll();
		return;
	}

	u8 soundId = 0;
	if (Pico.ram[snd68000addr[0]] >= sndFirstID && Pico.ram[snd68000addr[0]] <= sndLastID) {
		soundId = Pico.ram[snd68000addr[0]];
	} else if (Pico.ram[snd68000addr[1]] >= sndFirstID && Pico.ram[snd68000addr[1]] <= sndLastID) {
		soundId = Pico.ram[snd68000addr[1]];
	}
	if (soundId==0) return;

	soundId -= sndFirstID;

	// External sound
	if (sndHandlers[prevSndId] != NULL)
		mmEffectRelease( sndHandlers[prevSndId] );
	sndHandlers[soundId] = mmEffect(soundId);
	
	prevSndId = soundId;
}

void SoundPlayZ80(void) {
	if (!playSound || sndZ80addr[0]==0) return;

	if (pauseZ80addr != 0) {
		if (Pico.zram[pauseZ80addr] == pauseID && !soundPaused) {
			snd().stopStream();
			mmEffectCancelAll();
			soundPaused = true;
			return;
		} else if (Pico.zram[pauseZ80addr] == unpauseID && soundPaused) {
			snd().beginStream();
			soundPaused = false;
			return;
		}
	}

	u8 soundId = 0;
	if (Pico.zram[sndZ80addr[0]] >= sndFirstID && Pico.zram[sndZ80addr[0]] <= sndLastID) {
		soundId = Pico.zram[sndZ80addr[0]];
	} else if (Pico.zram[sndZ80addr[1]] >= sndFirstID && Pico.zram[sndZ80addr[1]] <= sndLastID) {
		soundId = Pico.zram[sndZ80addr[1]];
	}
	if (soundId==0) return;

	soundId -= sndFirstID;

	// External sound
	if (sndHandlers[prevSndId] != NULL)
		mmEffectRelease( sndHandlers[prevSndId] );
	sndHandlers[soundId] = mmEffect(soundId);
	
	prevSndId = soundId;
}
