// diff3DUtil.h - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan 2019

#ifndef DIFF_3D_UTIL_H
#define DIFF_3D_UTIL_H

#include "diff3D.h"

typedef struct { V3I min, max; } MMV3I;


/***/

extern size_t setDefaultMap (D3MapElem *pM, const V3I *pD);

extern Bool32 initOrg (DiffOrg *pO, uint def, uint nP);

extern DiffScalar initIsoW (DiffScalar w[], DiffScalar r, uint nHood, uint f);

#endif // DIFF_3D_UTIL_H
