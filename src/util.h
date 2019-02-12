// util.h - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan 2019

#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

// General compiler tweaks
#pragma clang diagnostic ignored "-Wmissing-field-initializers"

#ifndef SWAP
#define SWAP(Type,a,b) { Type tmp= (a); (a)= (b); (b)= tmp; }
#endif
#ifndef MIN
#define MIN(a,b) (a)<(b)?(a):(b)
#endif
#ifndef MAX
#define MAX(a,b) (a)>(b)?(a):(b)
#endif
#ifndef TRUE
#define TRUE (1)
#endif
#ifndef FALSE
#define FALSE (0)
#endif

// Terse type names
typedef signed char  I8;
typedef signed short I16;
typedef signed long  I32;

typedef unsigned char  U8;
typedef unsigned short U16;
typedef unsigned long  U32;

typedef float F32;
typedef double F64;


typedef int Bool32;
typedef double SMVal; // Stat measure value - use widest available IEEE type supported by hardware
typedef struct { SMVal vMin, vMax; } MMSMVal;
typedef struct
{
   SMVal    m[3];
} StatMom1;

typedef struct
{
   SMVal m0, m1[2], m2[4];
} StatMom2;

typedef struct
{
   SMVal m0, m1[3], m2[6];
} StatMom3;

typedef struct
{
   SMVal m,v;
} StatRes1;

typedef struct
{
   size_t bytes;
   union { void *p; size_t w; };
} MemBuff;


/***/

extern Bool32 validBuff (const MemBuff *pB, size_t b);
extern void releaseMemBuff (MemBuff *pB);

extern const char *stripPath (const char *path);

extern size_t fileSize (const char * const path);
extern size_t loadBuff (void * const pB, const char * const path, const size_t bytes);
extern size_t saveBuff (const void * const pB, const char * const path, const size_t bytes);

extern SMVal deltaT (void);

extern void statMom1AddW (StatMom1 * const pS, const SMVal v, const SMVal w);
extern void statMom3AddW (StatMom3 * const pS, const SMVal x, const SMVal y, const SMVal z, const SMVal w);
extern uint statMom1Res1 (StatRes1 * const pR, const StatMom1 * const pS, const SMVal dof);
extern uint statMom3Res1 (StatRes1 r[3], const StatMom3 * const pS, const SMVal dof);

extern float binSize (char *pCh, size_t s);

extern uint bitCountZ (size_t u);

#endif // UTIL_H
