extern u32 g_timerBaseMS;
void Timer_Init();
#define MILLISECOND_COUNTER ( TIMER1_DATA + g_timerBaseMS )

#ifndef PROFILE
#define SetupProfile( )
#define BeginProfile( )
#define EndProfile(fname)
#else
#define SetupProfile( ) int ____startMS_ = 0;
#define BeginProfile( ) ____startMS_ = MILLISECOND_COUNTER;
#define EndProfile(fname) iprintf( "Function %s took %dms  \n", fname, ( MILLISECOND_COUNTER - ____startMS_ ) );
#endif
