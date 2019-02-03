/*
  header file for software emulation for FM sound generator

*/
#ifndef _H_FM_FM_
#define _H_FM_FM_

#include "../../config.h"

void YM2612Init(int baseclock, int rate);
void YM2612ResetChip();
void YM2612UpdateOne(short *buffer, int length, int stereo);

void YM2612Write(unsigned int a, unsigned int v);
unsigned char YM2612Read();

void YM2612PicoTick();
void YM2612PicoStateLoad();

#endif /* _H_FM_FM_ */
