// mapUtil.h - utilities for map representation

#ifndef MAP_UTIL_H
#define MAP_UTIL_H

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef signed int   Index;
typedef signed long  Stride; // NB 64bit under PGI
typedef signed long  Offset; //    "

typedef struct { Index x, y, z; } V3I;
typedef struct { V3I vMin, vMax; } MMV3I;

//typedef struct { float x, y, z; } V3F;

typedef struct
{
   V3I   def;
   Stride stride[3], step[26];
   MMV3I mm;
   size_t n;
} MapOrg;

/***/

// INLINE?
extern Offset dotS3 (Index x, Index y, Index z, const Stride s[3]);// { return( (size_t) (x*s[0]) + y*s[1] + z*s[2] ); }

extern void step6FromStride (Stride step[6], const Stride stride[3]);

extern void adjustMMV3I (MMV3I *pR, const MMV3I *pS, const I32 a);

extern size_t initMapOrg (MapOrg *pO, const V3I *pD);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // MAP_UTIL_H



