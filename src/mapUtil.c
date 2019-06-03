// mapUtil.c - utilities for map representation used within 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan-June 2019

#include "mapUtil.h"

/***/

I32 collapseDim (const Stride s[3], const V3I *pDef)
{
   if (0 != s[0])
   {
      I32 d= (pDef->x > 1) + (pDef->y > 1) + (pDef->z > 1);
      I32 v= ((pDef->x * pDef->y) == s[2]) + (pDef->x == s[1]) + (1 == s[0]) - 1;
      return(d - v);
   }
   return(-1);
} // collapseDim

Offset dotS3 (Index x, Index y, Index z, const Stride s[3]) { return( x*s[0] + y*s[1] + z*s[2] ); }

void step6FromStride (Stride step[6], const Stride stride[3])
{
   step[0]= -stride[0];
   step[1]=  stride[0];
   step[2]= -stride[1];
   step[3]=  stride[1];
   step[4]= -stride[2];
   step[5]=  stride[2];
} // step6FromStride

void adjustMMV3I (MMV3I *pR, const MMV3I *pS, const I32 a)
{
   pR->vMin.x= pS->vMin.x - a;
   pR->vMin.y= pS->vMin.y - a;
   pR->vMin.z= pS->vMin.z - a;
   pR->vMax.x= pS->vMax.x + a;
   pR->vMax.y= pS->vMax.y + a;
   pR->vMax.z= pS->vMax.z + a;
} // adjustMMV3I

// HACK - function from diff3D.c/.h (uses mapUtil.h) - need to refactor again at some point...
extern void diffSet6To26 (Stride s26[]);

size_t initMapOrg (MapOrg *pO, const V3I *pD)
{
   pO->def= *pD;

   pO->stride[0]= 1;
   pO->stride[1]= pO->def.x;
   pO->stride[2]= pO->def.x * pO->def.y;

   step6FromStride(pO->step, pO->stride);
   diffSet6To26(pO->step);
   //report(TRC,"initMapOrg() - s26m[]=\n");
   //for (int i=0; i<26; i++) { report(TRC,"%d\n", pO->step[i]); }

   pO->mm.vMin.x= pO->mm.vMin.y= pO->mm.vMin.z= 0;
   pO->mm.vMax.x= pO->def.x-1;
   pO->mm.vMax.y= pO->def.y-1;
   pO->mm.vMax.z= pO->def.z-1;

   pO->n= (size_t)pO->stride[2] * pO->def.z;
   return(pO->n);
} // initMapOrg
