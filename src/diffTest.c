#include "diff3D.h"

typedef int bool;

typedef struct
{
   DiffOrg        org;
   DiffScalar     *pSR[2];
   D3S6IsoWeights w[1];
   D3S6MapElem    *pM;
} DiffTestContext;


static DiffTestContext gCtx={0,};

/***/

bool initOrg (DiffOrg *pO, uint def, uint nP)
{
   size_t n= 1;

   //def= MAX(32,def);
   //nP= MAX(1,nP);
   pO->def.x= pO->def.y= pO->def.z= def; 
   pO->nPhase= nP;
   for (int i=0; i<DIFF_DIM; i++)
   {
      pO->stride[i]= n; // planar
      n*= def; 
   }
   pO->phaseStride= n; // planar

   // if (interleaved) { pC->org.phaseStride= 1; for (int i=0; i<DIFF_DIM; i++) { pC->org.stride[i]*= nP } }

   pO->step[0]= -pO->stride[0];
   pO->step[1]=  pO->stride[0];
   pO->step[2]= -pO->stride[1];
   pO->step[3]=  pO->stride[1];
   pO->step[4]= -pO->stride[2];
   pO->step[5]=  pO->stride[2];

   pO->n1F= n;
   n*= pO->nPhase;
   pO->n1B= n;
   return(n > 0);
} // initOrg

void initW (D3S6IsoWeights * pW, DiffScalar r)
{
   pW->w[1]= r * (256 / 6) / 256;
   //pW->w[1]= r / 6.0;
   pW->w[0]= 1 - 6 * pW->w[1];
   printf("initW() - w[]=%G,%G\n", pW->w[0], pW->w[1]);
} // initW

size_t setDefaultMap (D3S6MapElem *pM, const V3I *pD)
{
   const size_t nP= pD->x * pD->y;
   const size_t nV= nP * pD->z;
   const Stride s[2]= {pD->x,nP};
   uint x, y, z;

   for (z=1; z < pD->z-1; z++)
   {
      for (y=1; y < pD->y-1; y++)
      {
         for (x=1; x < pD->x-1; x++)
         {
            pM[x + y * s[0] + z * s[1]]= 0x3F;
         }
      }
   }
   {
      size_t b1P= sizeof(*pM) * nP;
      size_t onP= nP * (pD->z - 1);
      // set boundaries to zero
      memset(pM, 0x00, b1P);
      //printf("plane: %zu, %zu\n", nP, b1P);
      memset(pM+onP, 0x00, b1P);
   }
   for (z=0; z < pD->z; z++)
   {
      y= pD->y-1;
      for (x=0; x < pD->x; x++)
      {
         pM[x + 0 * s[0] + z * s[1]]= 0x00;
         pM[x + y * s[0] + z * s[1]]= 0x00;
      }
      x= pD->x-1;
      for (y=0; y < pD->y; y++)
      {
         pM[0 + y * s[0] + z * s[1]]= 0x00;
         pM[x + y * s[0] + z * s[1]]= 0x00;
      }
   }
#if 1
   size_t t= 0;
   for (size_t i=0; i < nV; i++) { t+= (0 == pM[i]); }
   return(t);
#endif
} // setDefaultMap

void defFields (DiffScalar * pS, const DiffOrg *pO, DiffScalar v) // v[])
{
   const size_t b1B= pO->n1B * sizeof(*pS);
   size_t i;
   V3I c;
   uint phase;

   memset(pS, 0, b1B);

   c.x= pO->def.x/2; c.y= pO->def.y/2; c.z= pO->def.z/2;
   i= c.x * pO->stride[0] + c.y * pO->stride[1] + c.z * pO->stride[2];
   phase= 0;
   do
   {
      pS[i]= v;//[phase];
      i+= pO->phaseStride;
   } while (++phase < pO->nPhase);
} // defFields

float d2F3 (float dx, float dy, float dz) { return( dx*dx + dy*dy + dz*dz ); }

void setDiffK (DiffScalar k[2], DiffScalar Dt, uint dim)
{
   const DiffScalar var= 4 * Dt;
   k[0]= 1.0 / pow(M_PI * var, dim / 2.0);
   k[1]= -1.0 / var;
} // setDiffK

DiffScalar initPhaseAnalytic (DiffScalar * pS, const DiffOrg *pO, const uint phase, const DiffScalar v, const DiffScalar Dt)
{
   if (phase < pO->nPhase)
   {
      const float scale= 0.5; //0.25;
      const float s2= scale * scale;
      DiffScalar k[2], t= 0;
      V3F c;
      V3I i;
      c.x= pO->def.x/2; c.y= pO->def.y/2; c.z= pO->def.z/2;

      setDiffK(k, Dt, 3);
      pS+= phase * pO->phaseStride;
      for (i.z=0; i.z < pO->def.z; i.z++)
      {
         for (i.y=0; i.y < pO->def.y; i.y++)
         {
            for (i.x=0; i.x < pO->def.x; i.x++)
            {
               float r2= s2 * d2F3( i.x - c.x, i.y - c.y, i.z - c.z );

               size_t j= i.x * pO->stride[0] + i.y * pO->stride[1] + i.z * pO->stride[2];
               pS[j]= v * k[0] * exp(r2 * k[1]);
            }
         }
      }
      for (size_t j=0; j < pO->n1F; j++) { t+= pS[j]; }
      return(t);
   }
   return(0);
} // initPhaseAnalytic

size_t saveBuff (const void * const pB, const char * const path, const size_t bytes)
{
   FILE *hF= fopen(path,"w");
   if (hF)
   {
      size_t r= fwrite(pB, 1, bytes, hF);
      fclose(hF);
      if (r == bytes) { return(r); }
   }
   return(0);
} // saveBuff

size_t saveSliceRGB (const char path[], const DiffScalar * pS, const DiffOrg *pO, const uint phase, const uint z)
{
   if ((phase < pO->nPhase) && (z < pO->def.z))
   {
      unsigned char *pB;
      const size_t n1P= pO->def.x * pO->def.y;
      size_t nB= n1P * 3;
      pB= malloc(nB);
      if (pB)
      {
         const DiffScalar eps= 1.0 / (1<<30);
         DiffScalar rm=0, m= -1;
         pS+= phase * pO->phaseStride + z * pO->stride[2];
         for (size_t i=0; i < n1P; i++)
         {
            size_t j= i * pO->stride[0];
            m= MAX(m, pS[j]);
         }
         if (m > 0) { rm= 1.0 / m; }
         for (size_t i=0; i < n1P; i++)
         {
            size_t j= i * pO->stride[0];
            size_t k= i * 3;
            unsigned char r= 0, g=0, b= 0;
            if (pS[j] > eps)
            {
               float t= rm * pS[j];
               r= g= 0x00 + t * 0xF0;
               b= 0x40 + t * 0xBF;
            }
            pB[k+0]= r;
            pB[k+1]= g;
            pB[k+2]= b;
         }
         nB= saveBuff(pB, path, nB);
         printf("saveSliceRGB(%s ) - (m=%G) %zu bytes\n", path, m, nB);
         free(pB);
         return(nB);
      }
   }
   return(0);
} // saveSliceRGB

DiffScalar sumN (const DiffScalar * pS, size_t n)
{
   DiffScalar s= 0;
   for (size_t i=0; i<n; i++) { s+= pS[i]; }
   return(s);
} // sumN

DiffScalar sumField (const DiffScalar * pS, const DiffOrg *pO, uint phase) // v[])
{
   if (phase < pO->nPhase) { return sumN(pS + pO->n1F * phase, pO->n1F); }
   return(0);
} // size_t

bool init (DiffTestContext *pC, uint def)
{
   if (initOrg(&(pC->org), def, 1))
   {
      size_t b1M= pC->org.n1F * sizeof(*(pC->pM));
      size_t b1B= pC->org.n1B * sizeof(*(pC->pSR));
      uint nB= 0;

      initW(pC->w, 1);
      pC->pM= malloc(b1M);
      if (pC->pM)
      {
         //memset(pC->pM, 0, b1M);
         //
         printf("pM: %p, %zuB, Boundary=%zuZ\n", pC->pM, b1M, setDefaultMap(pC->pM, &(pC->org.def)) );
         nB++;
      }
      if (nB > 0)
      {
         for (int i= 0; i<2; i++)
            { pC->pSR[i]= malloc(b1B); nB+= (NULL != pC->pSR[i]); }
      }
      return(3 == nB);
   }
   return(0);
} // init

void release (DiffTestContext *pC)
{
   if (pC->pM) { free(pC->pM); pC->pM= NULL; }
   for (int i= 0; i<2; i++) { if (pC->pSR[i]) { free(pC->pSR[i]); pC->pSR[i]= NULL; } }
} // release


int main (int argc, char *argv[])
{
   uint iT=0, iF= 0;
   int r= 0;
   if (init(&gCtx,1<<8))
   {
      const Index zSlice= gCtx.org.def.z / 2;
      defFields(gCtx.pSR[0], &(gCtx.org), 1); // 1, 16.0625
      printf("initPhaseAnalytic() %G\n", initPhaseAnalytic(gCtx.pSR[1], &(gCtx.org), 0, 0.125, 4.03125));
      saveSliceRGB("analytic.rgb", gCtx.pSR[1], &(gCtx.org), 0, zSlice);

      //pragma acc set device_type(acc_device_none)
      iT+= diffProcIsoD3S6M(gCtx.pSR[1], gCtx.pSR[0], &(gCtx.org), gCtx.w, gCtx.pM, 100);
      iF= iT & 1;
      saveSliceRGB("numerical.rgb", gCtx.pSR[iF], &(gCtx.org), 0, zSlice);

      printf("sum[%u]=%G (%G)\n", iT, sumField(gCtx.pSR[iF], &(gCtx.org), 0), sumField(gCtx.pSR[iF^1], &(gCtx.org), 0));
   }
   release(&gCtx);
   printf("Complete\n");
   return(r);
} // diffProcIsoD3S6M
