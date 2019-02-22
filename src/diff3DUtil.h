// diff3DUtil.h - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan 2019

#ifndef DIFF_3D_UTIL_H
#define DIFF_3D_UTIL_H

#include "diff3D.h"

#define D3UF_  0
#define D3UF_PERM_SAVE  (1<<7)

typedef struct { V3I c; float v; } MapSiteInfo;

typedef struct
{
   D3MapElem m, v;   // Mask and compare value
} D3MapKey;

typedef struct
{
   U8 method, nHood, maxPermLvl, flags;
   float param[2];
} RawTransMethodDesc;

typedef struct
{
   U8 nHood, maxPermSet, permAlign, pad[1];
   V3I site;
} RawTransInfo;


/***/

extern size_t dotS3 (Index x, Index y, Index z, const Stride s[3]);// { return( (size_t) (x*s[0]) + y*s[1] + z*s[2] ); }

extern void adjustMMV3I (MMV3I *pR, const MMV3I *pS, const I32 a);

extern size_t initDiffOrg (DiffOrg *pO, uint def, uint nP);

extern DiffScalar initIsoW (DiffScalar w[], DiffScalar r, uint nHood, uint f);

extern float setDefaultMap (D3MapElem *pM, const V3I *pD, const uint id);

extern Bool32 getProfileRM (RawTransMethodDesc *pRM, U8 id, U8 f);

extern float mapFromU8Raw (D3MapElem *pM, RawTransInfo *pRI, const MemBuff *pWS, const char *path, 
                              const RawTransMethodDesc *pRM, const DiffOrg *pO);

#endif // DIFF_3D_UTIL_H
