// diff3DUtil.h - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan 2019

#ifndef DIFF_3D_UTIL_H
#define DIFF_3D_UTIL_H

#include "diff3D.h"

typedef struct { V3I c; float v; } MapSiteInfo;

typedef struct
{
   D3MapElem m, v;   // Mask and compare value
} D3MapKey;

typedef struct
{
   U8 method, ext[3];
   float param[2];
} RawTransMethodDesc;

typedef struct
{
   V3I site;
} RawTransInfo;


/***/

extern size_t dotS3 (Index x, Index y, Index z, const Stride s[3]);// { return( (size_t) (x*s[0]) + y*s[1] + z*s[2] ); }

extern void adjustMMV3I (MMV3I *pR, const MMV3I *pS, const I32 a);

extern size_t initDiffOrg (DiffOrg *pO, uint def, uint nP);

extern DiffScalar initIsoW (DiffScalar w[], DiffScalar r, uint nHood, uint f);

extern float setDefaultMap (D3MapElem *pM, const V3I *pD, const uint id);

extern float mapFromU8Raw (D3MapElem *pM, RawTransInfo *pRI, const MemBuff *pWS, const char *path, 
                              const RawTransMethodDesc *pRM, const DiffOrg *pO);

#endif // DIFF_3D_UTIL_H
