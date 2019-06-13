// util.h - miscelleaneous utility code
// https://github.com/DrAl-HFS/*.git
// (c) Project Contributors Feb 2018 - June 2019

#ifndef UTIL_H
#define UTIL_H

#include "report.h"

#ifdef __cplusplus
extern "C" {
#endif

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

extern Bool32 validMemBuff (const MemBuff *pB, size_t bytes);
extern Bool32 allocMemBuff (MemBuff *pB, size_t bytes);
extern void releaseMemBuff (MemBuff *pB);
extern Bool32 adjustMemBuff (MemBuff *pR, const MemBuff *pB, size_t skipS, size_t skipE);

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

// DISPLACE MATH/COMP TOOLS ? INLINE? 
extern F64 lcombF64 (const F64 x0, const F64 x1, const F64 r0, const F64 r1); // return(x0 * r0 + x1 * r1);
extern F64 lerpF64 (const F64 x0, const F64 x1, const F64 r0); // return(x0 * r0 + x1 * (1-r0));


extern int utilTest (void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // UTIL_H
