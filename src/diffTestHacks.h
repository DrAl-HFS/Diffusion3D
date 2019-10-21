// diffTestHacks.h - Dumping ground for hacky test code.
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan-Oct 2019

#ifndef DIFF_TEST_HACKS_H
#define DIFF_TEST_HACKS_H

#include "diff3D.h"

typedef struct
{
   U32 nHood, iter, mIvl, samples;
   DiffScalar  rD;
} TestParam;

typedef struct
{
   float *pF;
   int maxF, nF;
} FBuf;

extern int dumpScalarRegion (char *pCh, int maxCh, const DiffScalar * pS, const MMV3I *pRegion, const Stride s[3]);
extern int dumpMapRegion (char *pCh, int maxCh, const D3MapElem * pM, const MMV3I *pRegion, const Stride s[3]);
extern void dumpSMR (const DiffScalar * pS, const D3MapElem * pM, const V3I *pC, int a, const DiffOrg *pO);


extern float *allocNF (FBuf *pFB, const int n);

extern void addFB (FBuf *pFB, const float v);

#endif // DIFF_TEST_HACKS_H
