// hostFMA.c - wrapper for host-side (Minkowski) Functional Measure Analysis.
// https://github.com/DrAl-HFS/MKF.git
// (c) Project Contributors June-October 2019

#include "hostFMA.h"
//#include "diff3D.h"
//#include "mkfACC.h"

//define HOST_FMA_DISABLE
/*
typedef struct
{
   size_t      bpfd[MKF_BINS];
   MKFAccBinMap map;
   BMPackWord * pW;
   int         def[3];
   float       mScale;
} HostFMA;
*/

/***/

Bool32 hostSetupFMA (HostFMA *pC, const char relOpr[], const DiffScalar t, const float fScale, const DiffOrg *pO)
{
#ifndef HOST_FMA_DISABLE
   setupAcc(0,0); // set default to host side (multicore) acceleration
   if (NULL == pC->pW)
   {
      memcpy(pC->def, &(pO->def), sizeof(pO->def));
      pC->bytesW= sizeof(BMPackWord) * setBMO(NULL, pC->def, 0);
      //LOG("setBMO() [%d*%d*%d] -> bytesW=%zu\n", pC->def[0], pC->def[1], pC->def[2], pC->bytesW);
      pC->pW= malloc(pC->bytesW);
      if (pC->pW)
      {  //size_t a= (size_t)(pC->pW); if (a & 0x3) { WARN("**UNALIGNED! %p\n", a); }
         pC->fScale= fScale;
         if (setBinMapF64(&(pC->map), relOpr, t)) { return(TRUE); }
      } else { pC->bytesW= 0; }
   }
#endif // HOST_FMA_DISABLE
   return(FALSE);
} // hostSetupFMA

void hostTeardownFMA (HostFMA *pC)
{
#ifndef HOST_FMA_DISABLE
   if (pC->pW) { free(pC->pW); pC->pW= NULL; }
   setupAcc(1,1); // POP ?
#endif // HOST_FMA_DISABLE
} // hostTeardownFMA

Bool32 hostAnalyse (float *pM4, const HostFMA *pC, const DiffScalar *pF)
{
#ifndef HOST_FMA_DISABLE
   if (pM4 && mkfAccGetBPFDSimple(pC->bpfd, pC->pW, pF, pC->def, &(pC->map)))
   {
      mkfRefMeasureBPFD(pM4, pC->bpfd, pC->fScale);
      return(TRUE);
   }
#endif // HOST_FMA_DISABLE
   return(FALSE);
} // hostAnalyse

