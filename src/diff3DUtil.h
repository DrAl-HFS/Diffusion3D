// diff3DUtil.h - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan 2019

#ifndef DIFF_3D_UTIL_H
#define DIFF_3D_UTIL_H

#include "diff3D.h"

typedef struct { V3I vMin, vMax; } MMV3I;
typedef struct { V3I m; } MapInfo;


/***/

extern void adjustMMV3I (MMV3I *pR, const MMV3I *pS, const I32 a);

extern size_t initDiffOrg (DiffOrg *pO, uint def, uint nP);

extern DiffScalar initIsoW (DiffScalar w[], DiffScalar r, uint nHood, uint f);

extern float setDefaultMap (D3MapElem *pM, const V3I *pD, const uint id);

extern float mapFromU8Raw (D3MapElem *pM, MapInfo *pMI, const MemBuff *pWS, const char *path, U8 t, const DiffOrg *pO);

#endif // DIFF_3D_UTIL_H
