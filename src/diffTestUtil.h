// diffTestUtil.h - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan 2019

#ifndef DIFF_TEST_UTIL_H
#define DIFF_TEST_UTIL_H

#include "diff3DUtil.h"

extern const float gEpsilon; //= 1.0 / (1<<30);

typedef struct
{
   SMVal Dt, sad;
} SearchResult;


/***/

extern void defFields (DiffScalar * pS, const DiffOrg *pO, DiffScalar v);

extern float d2F3 (float dx, float dy, float dz);
extern float setDiffIsoK (DiffScalar k[2], const DiffScalar Dt, const uint dim);

extern DiffScalar compareAnalytic (DiffScalar * restrict pTR, const DiffScalar * pS, const DiffOrg *pO, const DiffScalar v, const DiffScalar Dt);
extern DiffScalar initAnalytic (DiffScalar * pR, const DiffOrg *pO, const DiffScalar v, const DiffScalar Dt);
extern DiffScalar analyseField (StatRes1 r[3], const DiffScalar * pS, const DiffOrg *pO);

extern size_t saveSliceRGB (const char path[], const DiffScalar * pS, const uint phase, const uint z, const DiffOrg *pO, const MMSMVal *pMM);

extern DiffScalar sumStrideNS (const DiffScalar * pS, const size_t n, const Stride s);
extern DiffScalar sumField (const DiffScalar * pS, const int phase, const DiffOrg *pO);
extern SMVal diffStrideNS (DiffScalar * pR, const DiffScalar * pS1, const DiffScalar * pS2, const size_t n, const Stride s);
extern SMVal relDiffStrideNS (DiffScalar * pR, const DiffScalar * pS1, const DiffScalar * pS2, const size_t n, const Stride s);

extern DiffScalar searchMin1 
(
   SearchResult *pR, const MemBuff * pWS,
   const DiffScalar *pS, const DiffOrg *pO, 
   const DiffScalar ma, const DiffScalar estDt, const uint f
);

typedef struct { SMVal sum; MMSMVal mm; } RedRes;
extern void reducto (RedRes * pR, const DiffScalar * const pS, const size_t n);

// DEPRECATE
extern DiffScalar searchNewton (const MemBuff * pWS, const DiffScalar *pS, const DiffOrg *pO, const DiffScalar ma, const DiffScalar estDt);

#endif // DIFF_TEST_UTIL_H
