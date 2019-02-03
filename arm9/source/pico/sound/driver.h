#ifndef __GNUC__
#pragma warning (disable:4100)
#pragma warning (disable:4244)
#pragma warning (disable:4245)
#pragma warning (disable:4710)
#pragma warning (disable:4018) // signed/unsigned
#endif

/* compiler dependence */
#ifndef OSD_CPU_H
#define OSD_CPU_H
typedef unsigned char	UINT8;   /* unsigned  8bit */
typedef unsigned short	UINT16;  /* unsigned 16bit */
typedef unsigned int	UINT32;  /* unsigned 32bit */
typedef signed char		INT8;    /* signed  8bit   */
typedef signed short	INT16;   /* signed 16bit   */
typedef signed int		INT32;   /* signed 32bit   */
#endif

#define LSB_FIRST 1

#ifndef INLINE
#define INLINE static __inline
#endif


#ifdef __DEBUG_PRINT
// External:
//int dprintf(char *Format, ...);
void dprintf2(char *format, ...);
#endif

