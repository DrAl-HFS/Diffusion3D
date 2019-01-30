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


INLINE void setS6M (Stride s6m[6], const Stride step[6], const uint m)
{
   //for (int i=0; i<DIFF_DIR; i++) { s[i]= ((1<<i) & m) ? pO->wraps6[i] : 0; }
   s6m[0]= (0x01 & m) ? step[0] : 0; // -X
   s6m[1]= (0x02 & m) ? step[1] : 0; // +X
   s6m[2]= (0x04 & m) ? step[2] : 0; // -Y
   s6m[3]= (0x08 & m) ? step[3] : 0; // +Y
   s6m[4]= (0x10 & m) ? step[4] : 0; // -Z
   s6m[5]= (0x20 & m) ? step[5] : 0; // +Z
} // setS6M

INLINE void setS8M (Stride s8m[8], const Stride step[6], const uint m)
{
   //for (int i=0; i<DIFF_DIR; i++) { s[i]= ((1<<i) & m) ? pO->wraps8[i] : 0; }
   s8m[0]= (0x01 & m) ? step[0] - step[3] - step[5] : 0; // -X -Y -Z
   s8m[1]= (0x02 & m) ? step[0] - step[3] + step[5] : 0; // -X -Y +Z
   s8m[2]= (0x04 & m) ? step[0] + step[3] + step[5] : 0; // -X +Y +Z
   s8m[3]= (0x08 & m) ? step[0] + step[3] - step[5] : 0; // -X +Y -Z
   s8m[4]= (0x10 & m) ? step[1] - step[3] - step[5] : 0; // +X -Y -Z
   s8m[5]= (0x20 & m) ? step[1] - step[3] + step[5] : 0; // +X -Y +Z
   s8m[6]= (0x40 & m) ? step[1] + step[3] + step[5] : 0; // +X +Y +Z
   s8m[7]= (0x80 & m) ? step[1] + step[3] - step[5] : 0; // +X +Y -Z
} // setS8M

INLINE void setS12M (Stride s12m[12], const Stride step[6], const uint m)
{
   //for (int i=0; i<DIFF_DIR; i++) { s[i]= ((1<<i) & m) ? pO->wraps8[i] : 0; }
   s12m[0]= ((0x01) & m) ? step[0] - step[3] : 0; // -X -Y
} // setS8M

//---

INLINE DiffScalar diffuseD3S6M
(
   const DiffScalar * pS, 
   const Stride     s6m[DIFF_DIR], 
   const DiffScalar w[2]
)
{
   return( w[0] * pS[0] +
            w[1] * ( pS[ s6m[0] ] + pS[ s6m[1] ] + pS[ s6m[2] ] +
                     pS[ s6m[3] ] + pS[ s6m[4] ] + pS[ s6m[5] ] )
          );
} // diffuseD3S6M

void procD3S6M
(
   DiffScalar * restrict pR,  // Result field(s)
   const DiffScalar  * const pS, // Source field(s)
   const DiffOrg     * const pO, // description
   const D3S6IsoWeights * const pW,
   const D3S6MapElem    * const pM
)
{
   #pragma acc data present( pR[:pO->n1B], pS[:pO->n1B], pO[:1], pW[:pO->nPhase], pM[:pO->n1F] )
   {
      for (Index z=0; z < pO->def.z; z++)
      {
         #pragma acc parallel loop
         for (Index y=0; y < pO->def.y; y++)
         {
            #pragma acc loop vector
            for (Index x=0; x < pO->def.x; x++)
            {
               const uint m= pM[x + pO->def.x * (y + (size_t) pO->def.y * z) ];
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
} // procD3S6M

//---

INLINE void setS14M (Stride s16m[16], const Stride step[6], const D3S14MapElem m)
{
   setS6M(s16m, step, m);
   setS8M(s16m+6, step, m>>6);
} // setS14M

INLINE DiffScalar diffuseD3S14M
(
   const DiffScalar * pS, 
   const Stride     s14m[6], 
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

void procD3S14M
(
   DiffScalar * restrict pR,  // Result field(s)
   const DiffScalar  * const pS, // Source field(s)
   const DiffOrg     * const pO, // description
   const D3S14IsoWeights * const pW,
   const D3S14MapElem    * const pM
)
{
   #pragma acc data present( pR[:pO->n1B], pS[:pO->n1B], pO[:1], pW[:pO->nPhase], pM[:pO->n1F] )
   {
      for (Index z=0; z < pO->def.z; z++)
      {
         #pragma acc parallel loop
         for (Index y=0; y < pO->def.y; y++)
         {
            #pragma acc loop vector
            for (Index x=0; x < pO->def.x; x++)
            {
               const uint m= pM[x + pO->def.x * (y + (size_t) pO->def.y * z) ];
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


/***/

uint diffProcIsoD3S6M
(
   DiffScalar * restrict pR,  // Result field(s)
   DiffScalar * restrict pS, // Source field(s)
   const DiffOrg        * pO, // descriptor
   const D3S6IsoWeights * pW,
   const D3S6MapElem    * pM,
   const uint nI
)
{
   uint i;
   #pragma acc data present_or_copyin( pO[:1], pW[:pO->nPhase], pM[:pO->n1F] )
   {
      if (0 == (nI & 1))
      {
         #pragma acc data present_or_create( pR[:pO->n1B] ) copy( pS[:pO->n1B] )
         {
            for (i= 0; i < nI; i+=2 )
            {
               procD3S6M(pR,pS,pO,pW,pM);
               procD3S6M(pS,pR,pO,pW,pM);
            }
         }
      }
      else
      {
         #pragma acc data present_or_create( pR[:pO->n1B] ) copyin( pS[:pO->n1B] ) copyout( pR[:pO->n1B] )
         {
            procD3S6M(pR,pS,pO,pW,pM);
            for (i= 1; i < nI; i+= 2 )
            {
               procD3S6M(pS,pR,pO,pW,pM);
               procD3S6M(pR,pS,pO,pW,pM);
            }
         }
      }
   }
   return(i);
} // diffProcIsoD3S6M

uint diffProcIsoD3S14M
(
   DiffScalar * restrict pR,  // Result field(s)
   DiffScalar * restrict pS, // Source field(s)
   const DiffOrg        * pO, // descriptor
   const D3S14IsoWeights * pW,
   const D3S14MapElem    * pM,
   const uint nI
)
{
   uint i;
   #pragma acc data present_or_copyin( pO[:1], pW[:pO->nPhase], pM[:pO->n1F] )
   {
      if (0 == (nI & 1))
      {
         #pragma acc data present_or_create( pR[:pO->n1B] ) copy( pS[:pO->n1B] )
         {
            for (i= 0; i < nI; i+=2 )
            {
               procD3S14M(pR,pS,pO,pW,pM);
               procD3S14M(pS,pR,pO,pW,pM);
            }
         }
      }
      else
      {
         #pragma acc data present_or_create( pR[:pO->n1B] ) copyin( pS[:pO->n1B] ) copyout( pR[:pO->n1B] )
         {
            procD3S14M(pR,pS,pO,pW,pM);
            for (i= 1; i < nI; i+= 2 )
            {
               procD3S14M(pS,pR,pO,pW,pM);
               procD3S14M(pR,pS,pO,pW,pM);
            }
         }
      }
   }
   return(i);
} // diffProcIsoD3S14M
