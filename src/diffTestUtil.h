// diffTestUtil.h - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan 2019

#ifndef DIFF_TEST_UTIL_H
#define DIFF_TEST_UTIL_H

#include "diff3D.h"


extern const float gEpsilon; //= 1.0 / (1<<30);


/***/

extern Bool32 initOrg (DiffOrg *pO, uint def, uint nP);
extern void initW (D3S6IsoWeights * pW, DiffScalar r);
extern size_t setDefaultMap (D3S6MapElem *pM, const V3I *pD);
extern void defFields (DiffScalar * pS, const DiffOrg *pO, DiffScalar v);

extern float d2F3 (float dx, float dy, float dz);
extern void setDiffK (DiffScalar k[2], const DiffScalar Dt, const uint dim);
extern DiffScalar compareAnalytic (const DiffScalar * pS, const DiffOrg *pO, const DiffScalar v, const DiffScalar Dt);
extern DiffScalar initPhaseAnalytic (DiffScalar * pR, const DiffOrg *pO, const uint phase, const DiffScalar v, const DiffScalar Dt);

extern size_t saveSliceRGB (const char path[], const DiffScalar * pS, const uint phase, const uint z, const DiffOrg *pO);

extern DiffScalar sumStrideNS (const DiffScalar * pS, const size_t n, const Stride s);
extern DiffScalar sumField (const DiffScalar * pS, const int phase, const DiffOrg *pO);
extern DiffScalar diffStrideNS (DiffScalar * pR, const DiffScalar * pS1, const DiffScalar * pS2, const size_t n, const Stride s);
extern DiffScalar relDiffStrideNS (DiffScalar * pR, const DiffScalar * pS1, const DiffScalar * pS2, const size_t n, const Stride s);

extern DiffScalar searchMin1 (const DiffScalar *pS, const DiffOrg *pO, const DiffScalar ma, const DiffScalar Dt0, const DiffScalar Dt1);
extern DiffScalar searchNewton (const DiffScalar *pS, const DiffOrg *pO, const DiffScalar ma, const DiffScalar Dt0, const DiffScalar Dt1);

#endif // DIFF_TEST_UTIL_H
