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


INLINE void setS6M (Stride s6m[6], const Stride step[6], const D3S6MapElem m)
{
   //for (int i=0; i<6; i++) { s[i]= ((1<<i) & m) ? pO->wraps[i] : 0; }
   s6m[0]= ((0x01) & m) ? step[0] : 0;
   s6m[1]= ((0x02) & m) ? step[1] : 0;
   s6m[2]= ((0x04) & m) ? step[2] : 0;
   s6m[3]= ((0x08) & m) ? step[3] : 0;
   s6m[4]= ((0x10) & m) ? step[4] : 0;
   s6m[5]= ((0x20) & m) ? step[5] : 0;
} // setS6M

INLINE DiffScalar diffuseD3S6M
(
   const DiffScalar * pS, 
   const Stride     s6m[6], 
   const DiffScalar w[2]
)
{
   return( pS[0] * w[0] +
           (pS[ s6m[0] ] + pS[ s6m[1] ] + pS[ s6m[2] ] + pS[ s6m[3] ] + pS[ s6m[4] ] + pS[ s6m[5] ]) * w[1] );
} // diffuseD3S6M

void proc
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
               const D3S6MapElem m= pM[x + pO->def.x * (y + (size_t) pO->def.y * z) ];
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
} // proc


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
               proc(pR,pS,pO,pW,pM);
               proc(pS,pR,pO,pW,pM);
            }
         }
      }
      else
      {
         #pragma acc data present_or_create( pR[:pO->n1B] ) copyin( pS[:pO->n1B] ) copyout( pR[:pO->n1B] )
         {
            proc(pR,pS,pO,pW,pM);
            for (i= 1; i < nI; i+= 2 )
            {
               proc(pS,pR,pO,pW,pM);
               proc(pR,pS,pO,pW,pM);
            }
         }
      }
   }
   return(i);
} // diffProcIsoD3S6M
