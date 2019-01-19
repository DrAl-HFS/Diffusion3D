// diffTestUtil.h - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan 2019

#ifndef DIFF_TEST_UTIL_H
#define DIFF_TEST_UTIL_H

#include "diff3D.h"

#if 0 //ndef UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#endif

#ifndef MAX
#define MAX(a,b) (a)>(b)?(a):(b)
#endif


typedef int bool;



/***/

extern bool initOrg (DiffOrg *pO, uint def, uint nP);
extern void initW (D3S6IsoWeights * pW, DiffScalar r);
extern size_t setDefaultMap (D3S6MapElem *pM, const V3I *pD);
extern void defFields (DiffScalar * pS, const DiffOrg *pO, DiffScalar v);

extern float d2F3 (float dx, float dy, float dz);
extern void setDiffK (DiffScalar k[2], DiffScalar Dt, uint dim);

extern DiffScalar initPhaseAnalytic (DiffScalar * pS, const DiffOrg *pO, const uint phase, const DiffScalar v, const DiffScalar Dt);

extern size_t saveBuff (const void * const pB, const char * const path, const size_t bytes);
extern size_t saveSliceRGB (const char path[], const DiffScalar * pS, const uint phase, const uint z, const DiffOrg *pO);

extern DiffScalar sumStrideNS (const DiffScalar * pS, const size_t n, const Stride s);
extern DiffScalar sumField (const DiffScalar * pS, const int phase, const DiffOrg *pO);
extern DiffScalar diffStrideNS (DiffScalar * pR, const DiffScalar * pS1, const DiffScalar * pS2, const size_t n, const Stride s);

#endif // DIFF_TEST_UTIL_H
