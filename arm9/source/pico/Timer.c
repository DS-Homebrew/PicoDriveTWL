#include <nds.h>

/* Lazy:
 * Since TIMER1_DATA will overflow in 65536 milliseconds, it is necessary
 * to add 65536 to this variable every time TIMER1_DATA overflows.
 * So, the total time elapsed since Timer_Init in milliseconds is g_timerBaseMS + TIMER1_DATA.
 */
u32 g_timerBaseMS = 0;

/* nds_timer1_overflow:
 * Adds 65536 to the base millisecond counter so we
 * don't lose any time when TIMER1_DATA rolls over.
 */
static void nds_timer1_overflow( void ) {
   g_timerBaseMS+= 65536;
}

/* Timer_GetMS:
 * Returns: The time in milliseconds since Timer_Init was called.
 */
int Timer_GetMS( void ) {
   return g_timerBaseMS + TIMER1_DATA;
}

/* Timer_Sleep:
 * Waits until ( usec ) microseconds have passed.
 */
void Timer_Sleep( int usec ) {
   swiDelay( usec );
}

/* Timer_Init:
 * Initialize NDS hardware timers.
 */
void Timer_Init( void ) {
   /* Timer0 will overflow roughly every 0.98 milliseconds */
   TIMER0_CR = TIMER_ENABLE | TIMER_DIV_1;
   TIMER0_DATA = 32768;

   /* When timer0 overflows, TIMER1_DATA will be incremented by 1.
    * When timer1 overflows 65536 is added to g_timerBaseMS so we don't lose
    * any time.
    */
   TIMER1_CR = TIMER_CASCADE | TIMER_IRQ_REQ;
   TIMER1_DATA = 0;

   /* Set and enable the interrupts for timer 1 */
   irqSet( IRQ_TIMER1, nds_timer1_overflow );
   irqEnable( IRQ_TIMER1 );
}
