// diff3D.c - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan 2019

#include "diff3D.h"

#ifdef __PGI   // HACKY
#define INLINE inline
#endif

#ifndef INLINE
#define INLINE
#endif

typedef U32 (*DiffProcIsoMapFuncPtr)
(
   DiffScalar * restrict, DiffScalar * restrict pS, 
   const DiffOrg * pO, const D3IsoWeights * pW, const D3MapElem * pM
);

/***/

INLINE void setS6M (Stride s6m[], const Stride step[], const U32 m)
{  // NB: ORDERED BY OPPOSING (-+) PAIRS FOR EFFICIENT PROCESSING!
   s6m[0]= (0x01 & m) ? step[0] : 0; // -X
   s6m[1]= (0x02 & m) ? step[1] : 0; // +X
   s6m[2]= (0x04 & m) ? step[2] : 0; // -Y
   s6m[3]= (0x08 & m) ? step[3] : 0; // +Y
   s6m[4]= (0x10 & m) ? step[4] : 0; // -Z
   s6m[5]= (0x20 & m) ? step[5] : 0; // +Z
} // setS6M

INLINE void setS12M (Stride s12m[], const Stride step[], const U32 m)
{  // NB: ORDERED BY OPPOSING (-+) PAIRS FOR EFFICIENT PROCESSING!
   s12m[0]= (0x001 & m) ? (step[0] + step[2]) : 0; // -X -Y
   s12m[1]= (0x002 & m) ? (step[1] + step[3]) : 0; // +X +Y
   s12m[2]= (0x004 & m) ? (step[0] + step[3]) : 0; // -X +Y
   s12m[3]= (0x008 & m) ? (step[1] + step[2]) : 0; // +X -Y

   s12m[4]= (0x010 & m) ? (step[0] + step[4]) : 0; // -X -Z
   s12m[5]= (0x020 & m) ? (step[1] + step[5]) : 0; // +X +Z
   s12m[6]= (0x040 & m) ? (step[0] + step[5]) : 0; // -X +Z
   s12m[7]= (0x080 & m) ? (step[1] + step[4]) : 0; // +X -Z

   s12m[8]= (0x100 & m) ? (step[2] + step[4]) : 0; // -Y -Z
   s12m[9]= (0x200 & m) ? (step[3] + step[5]) : 0; // +Y +Z
   s12m[10]= (0x400 & m) ? (step[2] + step[5]) : 0; // -Y +Z
   s12m[11]= (0x800 & m) ? (step[3] + step[4]) : 0; // +Y -Z
} // setS12M

INLINE void setS8M (Stride s8m[], const Stride step[], const U32 m)
{  // NB: ORDERED BY OPPOSING (-+) PAIRS FOR EFFICIENT PROCESSING!
   s8m[0]= (0x01 & m) ? step[0] + step[2] + step[4] : 0; // -X -Y -Z
   s8m[1]= (0x02 & m) ? step[1] + step[3] + step[5] : 0; // +X +Y +Z
   s8m[2]= (0x04 & m) ? step[0] + step[2] + step[5] : 0; // -X -Y +Z
   s8m[3]= (0x08 & m) ? step[1] + step[3] + step[4] : 0; // +X +Y -Z
   s8m[4]= (0x10 & m) ? step[0] + step[3] + step[4] : 0; // -X +Y -Z
   s8m[5]= (0x20 & m) ? step[1] + step[2] + step[5] : 0; // +X -Y +Z
   s8m[6]= (0x40 & m) ? step[0] + step[3] + step[5] : 0; // -X +Y +Z
   s8m[7]= (0x80 & m) ? step[1] + step[2] + step[4] : 0; // +X -Y -Z
} // setS8M

INLINE void setS14M (Stride s14m[], const Stride step[], const U32 m)
{
   setS6M(s14m, step, m);
   setS8M(s14m+6, step, m>>18); // NOTE fixed location of corner flags!
} // setS14M

INLINE void setS18M (Stride s18m[], const Stride step[], const U32 m)
{
   setS6M(s18m, step, m);
   setS12M(s18m+6, step, m>>6);
} // setS18M

INLINE void setS26M (Stride s26m[], const Stride step[], const U32 m)
{
   setS6M(s26m, step, m);
   setS12M(s26m+6, step, m>>6);
   setS8M(s26m+18, step, m>>18);
} // setS26M

// Boundary flag routines: not used here but map flag ordering must be identical
// to usage for generating offsets as in setS*M() above.
INLINE U32 getBoundaryM6 (Index x, Index y, Index z, const MMV3I *pMM)
{
   U32 m6= 0;
   m6|= (x > pMM->vMin.x) << 0; // -X
   m6|= (x < pMM->vMax.x) << 1; // +X
   m6|= (y > pMM->vMin.x) << 2; // -Y
   m6|= (y < pMM->vMax.y) << 3; // +Y
   m6|= (z > pMM->vMin.z) << 4; // -Z
   m6|= (z < pMM->vMax.z) << 5; // +Z
   return(m6);
} // getBoundaryM6

INLINE U32 getBoundaryM12 (const U32 m6)
{
   U32 m12= 0;
   m12|= ((m6 & 0x01) && (m6 & 0x04)) << 0; // -X -Y
   m12|= ((m6 & 0x02) && (m6 & 0x08)) << 1; // +X +Y
   m12|= ((m6 & 0x01) && (m6 & 0x08)) << 2; // -X +Y
   m12|= ((m6 & 0x02) && (m6 & 0x04)) << 3; // +X -Y
   m12|= ((m6 & 0x01) && (m6 & 0x10)) << 4; // -X -Z
   m12|= ((m6 & 0x02) && (m6 & 0x20)) << 5; // +X +Z
   m12|= ((m6 & 0x01) && (m6 & 0x20)) << 6; // -X +Z
   m12|= ((m6 & 0x02) && (m6 & 0x10)) << 7; // +X -Z
   m12|= ((m6 & 0x04) && (m6 & 0x10)) << 8;  // -Y -Z
   m12|= ((m6 & 0x08) && (m6 & 0x20)) << 9;  // +Y +Z
   m12|= ((m6 & 0x04) && (m6 & 0x20)) << 10; // -Y +Z
   m12|= ((m6 & 0x08) && (m6 & 0x10)) << 11; // +Y -Z
   return(m12);
} // getBoundaryM12

INLINE U32 getBoundaryM8 (const U32 m6)
{
   U32 m8= 0;
   m8|= ((m6 & 0x01) && (m6 & 0x04) && (m6 & 0x10)) << 0; // -X -Y -Z
   m8|= ((m6 & 0x02) && (m6 & 0x08) && (m6 & 0x20)) << 1; // +X +Y +Z
   m8|= ((m6 & 0x01) && (m6 & 0x04) && (m6 & 0x20)) << 2; // -X -Y +Z
   m8|= ((m6 & 0x02) && (m6 & 0x08) && (m6 & 0x10)) << 3; // +X +Y -Z
   m8|= ((m6 & 0x01) && (m6 & 0x08) && (m6 & 0x10)) << 4; // -X +Y -Z
   m8|= ((m6 & 0x02) && (m6 & 0x04) && (m6 & 0x20)) << 5; // +X -Y +Z
   m8|= ((m6 & 0x01) && (m6 & 0x08) && (m6 & 0x20)) << 6; // -X +Y +Z
   m8|= ((m6 & 0x02) && (m6 & 0x04) && (m6 & 0x10)) << 7; // +X -Y -Z
   return(m8);
} // getBoundaryM8


//---

INLINE DiffScalar diffuseD3S6M
(
   const DiffScalar * pS, 
   const Stride     s6m[6], 
   const DiffScalar w[2]
)
{
   return( w[0] * pS[0] +
            w[1] * ( pS[ s6m[0] ] + pS[ s6m[1] ] + pS[ s6m[2] ] +
                     pS[ s6m[3] ] + pS[ s6m[4] ] + pS[ s6m[5] ] )
          );
} // diffuseD3S6M

INLINE DiffScalar diffuseD3S14M
(
   const DiffScalar * pS, 
   const Stride     s14m[14], 
   const DiffScalar w[3]
)
{
   return( w[0] * pS[0] +
            w[1] * ( pS[ s14m[0] ] + pS[ s14m[1] ] + pS[ s14m[2] ] +
                     pS[ s14m[3] ] + pS[ s14m[4] ] + pS[ s14m[5] ] ) +
            w[2] * ( pS[ s14m[6] ] + pS[ s14m[7] ] + pS[ s14m[8] ] + pS[ s14m[9] ] + 
                     pS[ s14m[10] ] + pS[ s14m[11] ] + pS[ s14m[12] ] + pS[ s14m[13] ] )
          );
} // diffuseD3S14M

INLINE DiffScalar diffuseD3S18M
(
   const DiffScalar * pS, 
   const Stride     s18m[18], 
   const DiffScalar w[3]
)
{
   return( w[0] * pS[0] +
            w[1] * ( pS[ s18m[0] ] + pS[ s18m[1] ] + pS[ s18m[2] ] +
                     pS[ s18m[3] ] + pS[ s18m[4] ] + pS[ s18m[5] ] ) +
            w[2] * ( pS[ s18m[6] ] + pS[ s18m[7] ] + pS[ s18m[8] ] + pS[ s18m[9] ] + 
                     pS[ s18m[10] ] + pS[ s18m[11] ] + pS[ s18m[12] ] + pS[ s18m[13] ] +
                     pS[ s18m[14] ] + pS[ s18m[15] ] + pS[ s18m[16] ] + pS[ s18m[17] ] )
          );
} // diffuseD3S18M

INLINE DiffScalar diffuseD3S26M
(
   const DiffScalar * pS, 
   const Stride     s26m[26], 
   const DiffScalar w[4]
)
{
   return( w[0] * pS[0] +
            w[1] * ( pS[ s26m[0] ] + pS[ s26m[1] ] + pS[ s26m[2] ] +
                     pS[ s26m[3] ] + pS[ s26m[4] ] + pS[ s26m[5] ] ) +
            w[2] * ( pS[ s26m[6] ] + pS[ s26m[7] ] + pS[ s26m[8] ] + pS[ s26m[9] ] + 
                     pS[ s26m[10] ] + pS[ s26m[11] ] + pS[ s26m[12] ] + pS[ s26m[13] ] +
                     pS[ s26m[14] ] + pS[ s26m[15] ] + pS[ s26m[16] ] + pS[ s26m[17] ] ) +
            w[3] * ( pS[ s26m[18] ] + pS[ s26m[19] ] + pS[ s26m[20] ] + pS[ s26m[21] ] + 
                     pS[ s26m[22] ] + pS[ s26m[23] ] + pS[ s26m[24] ] + pS[ s26m[25] ] )
          );
} // diffuseD3S26M

//---


//---

void procD3S6M
(
   DiffScalar * restrict pR,  // Result field(s)
   const DiffScalar  * const pS, // Source field(s)
   const DiffOrg     * const pO, // description
   const D3IsoWeights * const pW,
   const D3MapElem    * const pM
)
{
   #pragma acc data present( pR[:pO->n1B], pS[:pO->n1B], pO[:1], pW[:pO->nPhase], pM[:pO->n1F] )
   { // collapse(2) -> 20*slower!?
      #pragma acc parallel loop
      for (Index z= 0; z < pO->def.z; z++)
      {
         for (Index y= 0; y < pO->def.y; y++)
         {
            #pragma acc loop vector
            for (Index x= 0; x < pO->def.x; x++)
            {
               const U32 m= pM[x + pO->def.x * (y + (size_t) pO->def.y * z) ];
               if (0 != m)
               {
                  const size_t i= x * pO->stride[0] + y * pO->stride[1] + z * pO->stride[2];
                  Stride s6m[6];
                  setS6M(s6m, pO->step, m);
                  //for (int i=0; i<6; i++) { printf("%d\n", s6m[i]); }
                  for (int phase= 0; phase < pO->nPhase; phase++)
                  {
                     size_t j= i + phase * pO->phaseStride;
                     pR[j]= diffuseD3S6M(pS+j, s6m, pW[phase].w);
                  }
               }
            }
         }
      }
   }
} // procD3S6M

//---

void procD3S14M
(
   DiffScalar * restrict pR,  // Result field(s)
   const DiffScalar  * const pS, // Source field(s)
   const DiffOrg     * const pO, // description
   const D3IsoWeights * const pW,
   const D3MapElem    * const pM
)
{
   #pragma acc data present( pR[:pO->n1B], pS[:pO->n1B], pO[:1], pW[:pO->nPhase], pM[:pO->n1F] )
   {
      #pragma acc parallel loop
      for (Index z= 0; z < pO->def.z; z++)
      {
         for (Index y= 0; y < pO->def.y; y++)
         {
            #pragma acc loop vector
            for (Index x= 0; x < pO->def.x; x++)
            {
               const U32 m= pM[x + pO->def.x * (y + (size_t) pO->def.y * z) ];
               if (0 != m)
               {
                  const size_t i= x * pO->stride[0] + y * pO->stride[1] + z * pO->stride[2];
                  Stride s14m[14];
                  setS14M(s14m, pO->step, m);
                  for (int phase= 0; phase < pO->nPhase; phase++)
                  {
                     size_t j= i + phase * pO->phaseStride;
                     pR[j]= diffuseD3S14M(pS+j, s14m, pW[phase].w);
                  }
               }
            }
         }
      }
   }
} // procD3S14M

//---

void procD3S18M
(
   DiffScalar * restrict pR,  // Result field(s)
   const DiffScalar  * const pS, // Source field(s)
   const DiffOrg     * const pO, // description
   const D3IsoWeights * const pW,
   const D3MapElem    * const pM
)
{
   #pragma acc data present( pR[:pO->n1B], pS[:pO->n1B], pO[:1], pW[:pO->nPhase], pM[:pO->n1F] )
   {
      #pragma acc parallel loop
      for (Index z= 0; z < pO->def.z; z++)
      {
         for (Index y= 0; y < pO->def.y; y++)
         {
            #pragma acc loop vector
            for (Index x= 0; x < pO->def.x; x++)
            {
               const U32 m= pM[x + pO->def.x * (y + (size_t) pO->def.y * z) ];
               if (0 != m)
               {
                  const size_t i= x * pO->stride[0] + y * pO->stride[1] + z * pO->stride[2];
                  Stride s18m[18];
                  setS18M(s18m, pO->step, m);
                  for (int phase= 0; phase < pO->nPhase; phase++)
                  {
                     size_t j= i + phase * pO->phaseStride;
                     pR[j]= diffuseD3S18M(pS+j, s18m, pW[phase].w);
                  }
               }
            }
         }
      }
   }
} // procD3S18M

//---

void procD3S26M
(
   DiffScalar * restrict pR,  // Result field(s)
   const DiffScalar  * const pS, // Source field(s)
   const DiffOrg     * const pO, // description
   const D3IsoWeights * const pW,
   const D3MapElem    * const pM
)
{
   #pragma acc data present( pR[:pO->n1B], pS[:pO->n1B], pO[:1], pW[:pO->nPhase], pM[:pO->n1F] )
   {
      #pragma acc parallel loop
      for (Index z= 0; z < pO->def.z; z++)
      {
         for (Index y= 0; y < pO->def.y; y++)
         {
            #pragma acc loop vector
            for (Index x= 0; x < pO->def.x; x++)
            {
               const U32 m= pM[x + pO->def.x * (y + (size_t) pO->def.y * z) ];
               if (0 != m)
               {
                  const size_t i= x * pO->stride[0] + y * pO->stride[1] + z * pO->stride[2];
                  Stride s26m[26];
                  setS26M(s26m, pO->step, m);
                  for (int phase= 0; phase < pO->nPhase; phase++)
                  {
                     size_t j= i + phase * pO->phaseStride;
                     pR[j]= diffuseD3S26M(pS+j, s26m, pW[phase].w);
                  }
               }
            }
         }
      }
   }
} // procD3S26M

//---

void procD3S6M8
(
   DiffScalar * restrict pR,  // Result field(s)
   const DiffScalar  * const pS, // Source field(s)
   const DiffOrg     * const pO, // description
   const D3S6IsoWeights  * const pW,
   const D3S6MapElem * const pM8
)
{
   #pragma acc data present( pR[:pO->n1B], pS[:pO->n1B], pO[:1], pW[:pO->nPhase], pM8[:pO->n1F] )
   {
      #pragma acc parallel loop
      for (Index z= 0; z < pO->def.z; z++)
      {
         for (Index y= 0; y < pO->def.y; y++)
         {
            #pragma acc loop vector
            for (Index x= 0; x < pO->def.x; x++)
            {
               const U32 m= pM8[x + pO->def.x * (y + (size_t) pO->def.y * z) ];
               if (0 != m)
               {
                  const size_t i= x * pO->stride[0] + y * pO->stride[1] + z * pO->stride[2];
                  //pR[i]= diffuseD3S6M(pS+i, pO->step, pW->w); // s6m, pW->w); //
                  Stride s6m[6];
                  setS6M(s6m, pO->step, m);
                  for (int phase= 0; phase < pO->nPhase; phase++)
                  {
                      size_t j= i + phase * pO->phaseStride;
                      pR[j]= diffuseD3S6M(pS+j, s6m, pW[phase].w);
                  }
               }
            }
         }
      }
   }
} // procD3S6M8


/***/

U32 diffProcIsoD3S6M8
(
   DiffScalar * restrict pR,  // Result field(s)
   DiffScalar * restrict pS, // Source field(s)
   const DiffOrg      * pO, // descriptor
   const D3S6IsoWeights  * pW,
   const D3S6MapElem * pM8,
   const U32 nI
)
{
   U32 i;
   #pragma acc data present_or_copyin( pO[:1], pW[:pO->nPhase], pM8[:pO->n1F] )
   {
      if (0 == (nI & 1))
      {
         #pragma acc data present_or_create( pR[:pO->n1B] ) copy( pS[:pO->n1B] )
         {
            for (i= 0; i < nI; i+=2 )
            {
               procD3S6M8(pR,pS,pO,pW,pM8);
               procD3S6M8(pS,pR,pO,pW,pM8);
            }
         }
      }
      else
      {
         #pragma acc data present_or_create( pR[:pO->n1B] ) copyin( pS[:pO->n1B] ) copyout( pR[:pO->n1B] )
         {
            procD3S6M8(pR,pS,pO,pW,pM8);
            for (i= 1; i < nI; i+= 2 )
            {
               procD3S6M8(pS,pR,pO,pW,pM8);
               procD3S6M8(pR,pS,pO,pW,pM8);
            }
         }
      }
   }
   return(i);
} // diffProcIsoD3S6M8


#ifdef DIFF_FUMEAN

#include "mkfCUDA.h"

typedef struct
{
   int iterNext, iterStep;
   BMFieldInfo inf;
   BMOrg bmo;
   BinMapF64 map;
   BMPackWord *pDevW;
   const void *pF[1];
   size_t bpfd[MKF_BINS]; // CAVEAT - presumably unpinned, therefore slow ?
   FMAPkt defP, *pP;
   int maxP, nP;
   float fScale;
} FMACtx;

FMACtx gAnCtx={ -1,-1, 0, };

static FMAPkt *nextPkt (FMACtx *pC)
{
   if (pC->pP && (pC->nP < pC->maxP))
   {
      return(pC->pP + pC->nP++); 
   }
   return( &(pC->defP) );
} // nextPkt

static Bool32 initFMACtx (FMACtx *pC, const DiffOrg *pO)
{
   size_t w=0;
   //#pragma acc host_data use_device(pF)
   //{ pC->pF[0]= pF; }
   pC->pF[0]= NULL;
   //pC->def[0]= pO->def.x; pC->def[1]= pO->def.y; pC->def[2]= pO->def.z;
   if (setupFields(&(pC->inf), pC->pF, 1, &(pO->def.x), sizeof(DiffScalar), 0))
   {
      //w= setBMO(&(pC->bmo), pC->inf.pD, 0);
      if (setBMO(&(pC->bmo), pC->inf.pD, 0)) //(w > 0)
      {
         //setBinMapF64(&(pC->map), ">", 0);
         pC->fScale= 3.0 / sumNI(pC->inf.pD, 3); // reciprocal mean
         //pC->pDevW= NULL; // bmcReset(pC->pW); ???
         return(TRUE);
      }
   }
   //else
   //ERROR_CALL("() %zu\n", w);
   //mkfCUDACleanup(1);
   return(FALSE);
} // initFMACtx

static Bool32 analyse (const DiffScalar *pF, int i) //, const DiffOrg *pO)
{
   FMACtx *pC= &gAnCtx;
   if (pC->iterNext == i) // initAnCtx(pC, pF, pO))
   {
      KernInfo ki;
      pC->iterNext+= pC->iterStep;
      #pragma acc host_data use_device(pF)
      { pC->pF[0]= pF; }
      if (pC->pDevW= binMapCUDA(&ki, pC->pDevW, &(pC->bmo), &(pC->inf), &(pC->map)))
      {
         FMAPkt *pP= nextPkt(pC);
         const size_t *pBPFD=NULL; //
         
         pBPFD= mkfCUDAGetBPFDH(&ki, NULL, &(pC->bmo), pC->pDevW, MKFCU_PROFILE_FAST);
         if (pBPFD)
         {
            mkfRefMeasureBPFD(pP->m, pBPFD, pC->fScale);

            pP->i= i;
            pP->dt[0]= 1E-3 * ki.dtms[0];
            pP->dt[1]= 1E-3 * ki.dtms[1];
            return(TRUE);
         }
      }
   }
   return(FALSE);
} // analyse

void diffSetIntervalFMA (int ivl)
{  FMACtx *pC= &gAnCtx; 
   if (ivl < 0) { pC->iterStep= pC->iterNext= -1; }
   else { pC->iterStep= ivl; }
} // diffSetFMAIvlPO2
void diffResetIter (int i) { FMACtx *pC= &gAnCtx; pC->iterNext= i * pC->iterStep; }

Bool32 diffSetupFMA (const int maxSamples, const char relOpr[], const DiffScalar t, const float fScale, const DiffOrg *pO)
{
   FMACtx *pC= &gAnCtx;
   if (pO)
   {
      if (!initFMACtx(pC, pO)) { return(FALSE); }
   }
   if ((NULL == pC->pP) && (maxSamples > 0) && (0 == pC->maxP))
   {
      pC->pP= malloc( maxSamples * sizeof(*(pC->pP)) );
      if (pC->pP) { pC->maxP= maxSamples; }
   }
   if (pC->pP)
   {
      diffSetIntervalFMA(-1); // disable
      return(NULL != setBinMapF64(&(pC->map), relOpr, t));
   }
   return(FALSE);
} // diffSetupFMA

int diffGetFMA (FMAPkt **ppAP, Bool32 reset)
{ 
   FMACtx *pC= &gAnCtx;
   int nP= pC->nP;
   if (pC->pP && ppAP) { *ppAP= pC->pP; }
   if (reset) { pC->nP= 0; }
   return(nP);
} // diffGetFMA

void diffTeardownFMA (void)
{  // release CUDA lazy alloc buffers
   FMACtx *pC= &gAnCtx;
   binMapCUDACleanup();
   mkfCUDACleanup(0);
   memset(pC, 0, sizeof(*pC));
   diffSetIntervalFMA(-1); // disable
} // diffTeardownFMA

#else // DIFF_FUMEAN

static Bool32 analyse (float m[4], const DiffScalar *pF, const DiffOrg *pO) { return(FALSE); }
void diffSetIntervalFMA (int ivl) { ; }
void diffResetIter (int i) { ; }
void diffTeardownFMA (void) { ; }

#endif // DIFF_FUMEAN


U32 diffProcIsoD3SxM
(
   DiffScalar * restrict pR,  // Result field(s)
   DiffScalar * restrict pS, // Source field(s)
   const DiffOrg        * pO, // descriptor
   const D3IsoWeights * pW,
   const D3MapElem    * pM,
   const U32 nI,
   const U32 nHood
)
{
   DiffProcIsoMapFuncPtr pF=NULL;
   U32 i= 0;
   switch(nHood)
   {
      case 26 : pF= (DiffProcIsoMapFuncPtr)procD3S26M; break;
      case 18 : pF= (DiffProcIsoMapFuncPtr)procD3S18M; break;
      case 14 : pF= (DiffProcIsoMapFuncPtr)procD3S14M; break;
      case 6  : pF= (DiffProcIsoMapFuncPtr)procD3S6M; break;
   }

   if (pF)
   #pragma acc data present_or_copyin( pO[:1], pW[:pO->nPhase], pM[:pO->n1F] )
   {
      diffResetIter(1);
      if (0 == (nI & 1))
      {
         #pragma acc data present_or_create( pR[:pO->n1B] ) copy( pS[:pO->n1B] )
         {
            //analyse(pS,0);
            for (i= 0; i < nI; i+=2 )
            {
               pF(pR,pS,pO,pW,pM);
               analyse(pR,i+1);
               pF(pS,pR,pO,pW,pM);
               analyse(pS,i+2);
            }
         }
      }
      else
      {
         #pragma acc data present_or_create( pR[:pO->n1B] ) copyin( pS[:pO->n1B] ) copyout( pR[:pO->n1B] )
         {
            //analyse(pS,0);
            pF(pR,pS,pO,pW,pM);
            analyse(pR,1);
            for (i= 1; i < nI; i+= 2 )
            {
               pF(pS,pR,pO,pW,pM);
               analyse(pS,i+1);
               pF(pR,pS,pO,pW,pM);
               analyse(pR,i+2);
            }
         }
      }
   }
   return(i);
} // diffProcIsoD3SxM

void diffSet6To26 (Stride s26[])
{
   setS12M(s26+6, s26, 0xFFF);
   setS8M(s26+18, s26, 0xFF);
} // diffSet6To26

U32 getBoundaryM26 (Index x, Index y, Index z, const MMV3I *pMM)
{
   const U32 m6= getBoundaryM6(x, y, z, pMM);
   return( m6 | (getBoundaryM12(m6) << 6) | (getBoundaryM8(m6) << 18) );
} // getBoundaryM26

U32 getBoundaryM26V (Index x, Index y, Index z, const MMV3I *pMM)
{
   const U32 m6= getBoundaryM6(x, y, z, pMM);
   const U32 m12= getBoundaryM12(m6);
   const U32 m8= getBoundaryM8(m6);
   report(TRC0,"getBoundaryM26V(%d, %d, %d) - m6=0x%x, m12=0x%x, m8=0x%x\n", x, y, z, m6, m12, m8);
   return(m6 | (m12 << 6) | (m8 << 18));
} // getBoundaryM26V

void test (const DiffOrg * pO)
{
   Stride s26m[26];

   setS26M(s26m+0, pO->step, 0x3FFFFFF); // (1<<26)-1
   report(TRC0,"s26m[]=\n");
   for (int i=0; i<26; i++) report(TRC0,"%d\n",s26m[i]);
} // test
