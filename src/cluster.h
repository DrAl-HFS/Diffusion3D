// ccl.h - Cluster extraction

#ifndef CLUSTER_H
#define CLUSTER_H

#include "diff3D.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef U32 ClustIdx;

typedef struct
{
   ClustIdx *pNI, *pNC;
   size_t   nNI, nNC, iNCMax;
} ClustRes;

//extern void testC (Stride s[3]);
extern size_t findNIMinD (const ClustIdx u[], const size_t n, const ClustIdx v); // Hacky 1D min search

extern void split3 (I32 v[3], ClustIdx o, const Stride stride[3]);

//extern U32 clusterXDYZBN6 (U32 *pI, U32 *pNIC, U8 *pU8, const V3I *pDef, const Stride stride[3]);

extern U32 clusterExtract (ClustRes *pR, const MemBuff *pWS, U8 *pImg, const V3I *pDef, const Stride stride[3]);

extern U32 clusterResGetMM (MMU32 *pMM, const ClustRes *pCR, U32 iC);

extern void clusterSortUA (ClustIdx u[], U32 n); // { qsort(u, n, sizeof(U32), {a-b} ); }
extern void clusterSortAbsID (I32 i[], U32 n); // { qsort(i, n, sizeof(I32), {abs(b)-abs(a)} ); }

extern void clusterOptimise (ClustIdx ni[], const size_t nNI);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // CLUSTER_H



