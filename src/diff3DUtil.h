// diff3DUtil.h - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan 2019

#ifndef DIFF_3D_UTIL_H
#define DIFF_3D_UTIL_H

#include "diff3D.h"

typedef struct { V3I min, max; } MMV3I;
typedef struct { V3I m; } MapInfo;


/***/

extern Bool32 initDiffOrg (DiffOrg *pO, uint def, uint nP);

extern DiffScalar initIsoW (DiffScalar w[], DiffScalar r, uint nHood, uint f);

extern size_t setDefaultMap (D3MapElem *pM, const V3I *pD);

extern size_t mapFromU8Raw (D3MapElem *pM, MapInfo *pMI, const MemBuff *pWS, const char *path, U8 t, const DiffOrg *pO);

#endif // DIFF_3D_UTIL_H
