// diffTestUtil.h - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan 2019

#ifndef DIFF_TEST_UTIL_H
#define DIFF_TEST_UTIL_H

#include "diff3DUtil.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const float gEpsilon; //= 1.0 / (1<<30);

typedef struct
{
   U8 vMin,vMax;
} MMU8;

typedef struct
{
   SMVal Dt, sad;
} SearchResult;

typedef struct
{
   D3MapKey k;   // Mask and compare value
   DiffScalar v2[2];
} D3MapKeyVal;

extern const D3MapKeyVal gDefObsKV; // = { {-1,0}, { 0, NAN } };

typedef struct
{
   SMVal mean[3], var[3];
   MMSMVal  mm;
   SMVal sum;
} AnResD3R2;


/***/

extern void initHack (void);

extern size_t initFieldVCM (DiffScalar * pS, const DiffOrg *pO, const D3MapElem *pM, const D3MapKeyVal *pKV, const MapSiteInfo * pI);
extern size_t resetFieldVCM (DiffScalar * pS, const DiffOrg *pO, const D3MapElem *pM, const D3MapKey *pK, DiffScalar v);

extern float d2F3 (float dx, float dy, float dz);
extern float setDiffIsoK (DiffScalar k[2], const DiffScalar Dt, const U32 dim);

extern DiffScalar compareAnalytic (DiffScalar * restrict pTR, const DiffScalar * pS, const DiffOrg *pO, const DiffScalar v, const DiffScalar Dt);
extern DiffScalar initAnalytic (DiffScalar * pR, const DiffOrg *pO, const DiffScalar v, const DiffScalar Dt);
extern DiffScalar analyseField (AnResD3R2 * pR, const DiffScalar * pS, const DiffOrg *pO);

extern size_t saveSliceRGB (const char path[], const DiffScalar * pS, const DiffOrg *pO, const MMSMVal *pMM);

extern DiffScalar searchMin1 
(
   SearchResult *pR, const MemBuff * pWS,
   const DiffScalar *pS, const DiffOrg *pO, 
   const DiffScalar ma, const DiffScalar estDt, const U32 f
);

typedef struct { SMVal sum; MMSMVal mm; } RedRes;
extern void reduct0 (RedRes * pR, const DiffScalar * const pS, const size_t n);
// Reduce field to plane, then plane to scalars
extern void reduct3_2_0 (RedRes * pR, DiffScalar * restrict pTR, const DiffScalar * const pS, const DiffOrg *pO, const char map);

// DEPRECATE
extern DiffScalar searchNewton (const MemBuff * pWS, const DiffScalar *pS, const DiffOrg *pO, const DiffScalar ma, const DiffScalar estDt);
/*
extern DiffScalar sumStrideNS (const DiffScalar * pS, const size_t n, const Stride s);
extern DiffScalar sumField (const DiffScalar * pS, const int phase, const DiffOrg *pO);
extern SMVal diffStrideNS (DiffScalar * pR, const DiffScalar * pS1, const DiffScalar * pS2, const size_t n, const Stride s);
extern SMVal relDiffStrideNS (DiffScalar * pR, const DiffScalar * pS1, const DiffScalar * pS2, const size_t n, const Stride s);
*/

extern void dumpM6 (U32 m6, const char *e);
//extern void dumpM12 (U32 m12, const char *e);
//extern void dumpM8 (U32 m8, const char *e);
extern void dumpDistBC (const D3MapElem * pM, const size_t nM);
extern void dumpDMMBC (const U8 *pU8, const D3MapElem * pM, const size_t n, const U32 mask);
extern void checkComb (const V3I v[2], const Stride stride[3], const D3MapElem * pM);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DIFF_TEST_UTIL_H
