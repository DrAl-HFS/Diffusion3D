// hostFMA.h - wrapper for host-side (Minkowski) Functional Measure Analysis.
// https://github.com/DrAl-HFS/MKF.git
// (c) Project Contributors June-October 2019

#ifndef HOST_FMA_H
#define HOST_FMA_H

#include "diff3D.h"
#include "mkfACC.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
   size_t      bpfd[MKF_BINS], bytesW;
   MKFAccBinMap map;
   BMPackWord * pW;
   int         def[3];
   float       mScale;
} HostFMA;


/***/

Bool32 hostSetupFMA (HostFMA *pC, const char relOpr[], DiffScalar t, const DiffOrg *pO);

void hostTeardownFMA (HostFMA *pC);

Bool32 hostAnalyse (float *pM4, const HostFMA *pC, const DiffScalar *pF);


#ifdef __cplusplus
} // extern "C"
#endif

#endif // HOST_FMA_H
