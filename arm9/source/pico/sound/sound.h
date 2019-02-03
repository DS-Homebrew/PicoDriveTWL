#ifdef __cplusplus
extern "C" {
#endif

int  sound_timers_and_dac(int raster);
int  sound_render();

// z80 functionality wrappers
void z80_init();
void z80_resetCycles();
void z80_int();
int  z80_run(int cycles);
void z80_exit();

#ifdef __cplusplus
} // End of extern "C"
#endif
