// util.h - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan-Mar 2019

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

#ifdef __cplusplus
extern "C" {
#endif

// General compiler tweaks
//pragma clang diagnostic ignored "-Wmissing-field-initializers"

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
typedef signed char       I8;
typedef signed short      I16;
typedef signed int        I32;
// Beware! sizeof(long)==8 under PGI!
typedef signed long long I64;

typedef unsigned char      U8;
typedef unsigned short     U16;
typedef unsigned int       U32;
typedef unsigned long long U64;

typedef float F32;
typedef double F64;


typedef int Bool32;
typedef double SMVal; // Stat measure value - use widest available IEEE type supported by hardware
typedef struct { U32   vMin, vMax; } MMU32; // Z size_t ???
typedef struct { I64   vMin, vMax; } MMI64;
typedef struct { SMVal vMin, vMax; } MMSMVal;
typedef struct
{
   SMVal    m[3];
} StatMomD1R2; // 1 dimensional, second order statistical moments

typedef struct
{
   SMVal m0, m1[2], m2[4];
} StatMomD2R2; // 2 dimensional, second order statistical moments

typedef struct
{
   SMVal m0, m1[3], m2[6];
} StatMomD3R2; // 3 dimensional, second order statistical moments

typedef struct
{
   SMVal m,v;
} StatResD1R2;

typedef struct
{
   size_t bytes;
   union { void *p; size_t w; };
} MemBuff;

typedef unsigned int MBVal; // Arg holder for multi-bit/byte read/write routines


/***/

extern Bool32 validBuff (const MemBuff *pB, size_t b);
extern void releaseMemBuff (MemBuff *pB);
extern Bool32 adjustBuff (MemBuff *pR, const MemBuff *pB, size_t skipS, size_t skipE);

extern const char *stripPath (const char *path);

extern size_t fileSize (const char * const path);
extern size_t loadBuff (void * const pB, const char * const path, const size_t bytes);
extern size_t saveBuff (const void * const pB, const char * const path, const size_t bytes);

// Read/write multi-byte quantities in little-endian order
extern MBVal readBytesLE (const U8 * const pB, const size_t idx, const U8 nB);
extern MBVal writeBytesLE (U8 * const pB, const size_t idx, const U8 nB, const MBVal v);

extern SMVal deltaT (void);

extern void statMom1AddW (StatMomD1R2 * const pS, const SMVal v, const SMVal w);
extern void statMom3AddW (StatMomD3R2 * const pS, const SMVal x, const SMVal y, const SMVal z, const SMVal w);
extern U32 statMom1Res1 (StatResD1R2 * const pR, const StatMomD1R2 * const pS, const SMVal dof);
extern U32 statMom3Res1 (StatResD1R2 r[3], const StatMomD3R2 * const pS, const SMVal dof);

extern float binSizeZ (char *pCh, size_t s);
extern float decSizeZ (char *pCh, size_t s);

extern U32 bitCountZ (size_t u);
extern I32 bitNumHiZ (size_t u); // Number of highest bit set or -1
extern U32 bitsReqI32 (I32 i); // Bits required to store value - NB - additional bit for sign flag / leading zero not included!

extern int strFmtNSMV (char s[], const int maxS, const char *fmt, const SMVal v[], const int n);

extern SMVal sumNSMV (const SMVal v[], const size_t n);
extern SMVal meanNSMV (const SMVal v[], const size_t n);

extern int utilTest (void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // UTIL_H
