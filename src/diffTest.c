// diffTest.c - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan 2019

#include "diffTestUtil.h"

typedef struct
{
   DiffOrg        org;
   DiffScalar     *pSR[2];
   D3IsoWeights   wPhase[1];
   D3MapElem      *pM;
   MemBuff        ws;
} DiffTestContext;


static DiffTestContext gCtx={0,};

const DiffScalar gM= 1.0;


/***/

Bool32 init (DiffTestContext *pC, uint def)
{
   if (initDiffOrg(&(pC->org), def, 1))
   {
      size_t b1M= pC->org.n1F * sizeof(*(pC->pM));
      size_t b1B= pC->org.n1B * sizeof(*(pC->pSR));
      size_t b1W= pC->org.n1F * sizeof(*(pC->pSR));
      uint nB= 0;

      pC->pM= malloc(b1M);
      pC->ws.bytes=  b1W; // 1<<28; ???
      pC->ws.p=  malloc(pC->ws.bytes);
      for (int i= 0; i<2; i++)
         { pC->pSR[i]= malloc(b1B); nB+= (NULL != pC->pSR[i]); }
      return(2 == nB);
   }
   return(0);
} // init

void release (DiffTestContext *pC)
{
   if (pC->pM) { free(pC->pM); pC->pM= NULL; }
   if (pC->ws.p) { free(pC->ws.p); pC->ws.p= NULL; }
   for (int i= 0; i<2; i++) { if (pC->pSR[i]) { free(pC->pSR[i]); pC->pSR[i]= NULL; } }
} // release

typedef struct
{
   const char *fs, *eol[2];
} FmtDesc;

int dumpRegion (char *pCh, int maxCh, const DiffScalar * pS, const MMV3I *pRegion, const Stride s[3])
{
   FmtDesc fd={"%.3G ","\n","\n"};
   int nCh= 0;
   for (Index z= pRegion->vMin.z; z <= pRegion->vMax.z; z++)
   {
      for (Index y= pRegion->vMin.y; y <= pRegion->vMax.y; y++)
      {
         for (Index x= pRegion->vMin.x; x <= pRegion->vMax.x; x++)
         {
            const size_t i= x * s[0] + y * s[1] + z * s[2];
            nCh+= snprintf(pCh+nCh, maxCh-nCh, fd.fs, pS[i]);
         }
         nCh+= snprintf(pCh+nCh, maxCh-nCh, fd.eol[0]);
      }
      nCh+= snprintf(pCh+nCh, maxCh-nCh, fd.eol[1]);
   }
   return(nCh);
} // dumpRegion

void dump (const DiffScalar * pS, const D3MapElem * pM, const V3I *pC, int a)
{
   const MMV3I mm= {pC->x-a,pC->y-a,pC->z-a, pC->x+a,pC->y+a,pC->z+a};
   char buff[1<<12];
   //adjustMMV3I(&mm, a);
   if (dumpRegion(buff, sizeof(buff)-1, pS, &mm, gCtx.org.stride))
   {
      printf("C(%d,%d,%d):-\n%s", pC->x, pC->y, pC->z, buff);
      for (Index z= mm.vMin.z; z <= mm.vMax.z; z++)
      {
         for (Index y= mm.vMin.y; y <= mm.vMax.y; y++)
         {
            for (Index x= mm.vMin.x; x <= mm.vMax.x; x++)
            {
               size_t i= x + y * gCtx.org.def.x + z * gCtx.org.def.x * gCtx.org.def.y;
               dumpM6(pM[i], ",\t");
               //printf("0x%x ", pM[i] & 0x3f);
            }
            printf("\n");
         }
         printf("\n");
      }
   }
} // dump

typedef struct
{
   uint nHood, iter;
   DiffScalar  rD;
} TestParam;

typedef struct
{
   SMVal          tProc;
   SearchResult  sr;
} Test1Res;

uint testAn (Test1Res *pR, const TestParam *pP)
{
   uint iT=0, iN= 0, iA= 0;

   if (initIsoW(gCtx.wPhase[0].w, pP->rD, pP->nHood, 0) > 0)
   {
      V3I c={ gCtx.org.def.x/2, gCtx.org.def.y/2, gCtx.org.def.z/2 };
      initFieldVC(gCtx.pSR[0], &(gCtx.org), gM, &c);
      deltaT();
      iT= diffProcIsoD3SxM(gCtx.pSR[1], gCtx.pSR[0], &(gCtx.org), gCtx.wPhase, gCtx.pM, pP->iter, pP->nHood);
      pR->tProc= deltaT();
      iN= iT & 1;
      // Search for Diffusion-time moment
      StatRes1 r[3];
      DiffScalar isovar= analyseField(r, gCtx.pSR[iN], &(gCtx.org));
      printf("analyseField() - iso=%G, m=%G,%G,%G, v=%G,%G,%G\n", isovar, r[0].m, r[1].m, r[2].m, r[0].v, r[1].v, r[2].v);
      DiffScalar Dt= sqrt(iT * 2 * pP->rD);
      Dt= searchMin1(&(pR->sr), &(gCtx.ws), gCtx.pSR[iN], &(gCtx.org), gM, 0.5 * isovar, 0);
   }
   return(iT);
} // testAn

void redCheck (MMSMVal *pMM, const DiffScalar *pN, const DiffScalar *pA, uint f)
{
   RedRes rrN, rrA;
   SMVal dt;

   deltaT();
   reduct0(&rrN, pN, gCtx.org.n1F);
   reduct0(&rrA, pA, gCtx.org.n1F);
   dt= deltaT();
   if (f & 1) { printf("sums: N=%G A=%G\n", rrN.sum, rrA.sum); }
   pMM->vMin= fmin(rrN.mm.vMin, rrA.mm.vMin);
   pMM->vMax= fmin(rrN.mm.vMax, rrA.mm.vMax);
   if (f & 1) { printf("MM: N=%G,%G A=%G,%G %Gsec\n", rrN.mm.vMin, rrN.mm.vMax, rrA.mm.vMin, rrA.mm.vMax, dt); }
} // redCheck

void save (const char *basePath, const char *baseName, const DiffScalar *pS, const uint nHood, const uint iter, const int z, const MMSMVal *pMM)
{
   char path[256];
   const int m= sizeof(path)-1;
   int n= 0;

   if (z >= 0)
   {
      n+= snprintf(path+n, m-n, "%s/N%02d_I%07d_Z%03d_%s", basePath, nHood, iter, z, baseName);
      saveSliceRGB(path, pS, z, &(gCtx.org), pMM);
   }
   else
   {
      n+= snprintf(path+n, m-n, "%s/N%02d_I%07d_%s", basePath, nHood, iter, baseName);
      saveSliceRGB(path, pS, 0, &(gCtx.org), pMM);
   }
} // save

void compareAnNHI (const U8 nHoods[], const U8 nNH, const uint step, const uint max)
{
   TestParam   param;
   Test1Res    res[4][5], *pR;
   uint        iT, iN, iA;

   for (U8 h=0; h<nNH; h++)
   {
      param.nHood= nHoods[h];
      pR= res[h];
      param.rD=    0.5;
      // warm-up (field not initialised)
      initIsoW(gCtx.wPhase[0].w, param.rD, param.nHood, 0);
      diffProcIsoD3SxM(gCtx.pSR[1], gCtx.pSR[0], &(gCtx.org), gCtx.wPhase, gCtx.pM, 2, param.nHood);
      printf("diffProc.. warmup OK\n");
      for (int i=step; i<=max; i+= step)
      {
         param.iter=  i;
         iT= testAn(pR, &param);
         if (iT >= max)
         {
            const Index zSlice= gCtx.org.def.z / 2;
            MMSMVal mm;

            printf("diffProc..%u %u iter %G sec (%G msec/iter) Dt=%G sad=%G\n", param.nHood, iT, pR->tProc, 1000*pR->tProc / iT, pR->sr.Dt, pR->sr.sad);
            iN= iT & 1;
            iA= iN ^ 1;
            initAnalytic(gCtx.pSR[iA], &(gCtx.org), gM, pR->sr.Dt);
            //redCheck(&mm, gCtx.pSR[iN], gCtx.pSR[iA], 1);
            save("rgb", "NUM.rgb", gCtx.pSR[iN], param.nHood, param.iter, zSlice, NULL);
            save("rgb", "ANL.rgb", gCtx.pSR[iA], param.nHood, param.iter, zSlice, NULL);
            save("rgb", "SAD.rgb", gCtx.ws.p, param.nHood, param.iter, -1, NULL);
         }
         pR++;
      }
   }
   //const SMVal tA= 0.05, rA= 1.0 / tA;
   printf("NH I\tt(sec)\tDt\tSAD\t\tAcc\tEff(Dt/t)\n");
   for (U8 h=0; h<nNH; h++)
   {
      pR= res[h];
      for (int i=step; i<=max; i+= step)
      {
         printf("%2u %3u\t%5.03G\t%5.03G\t%7.05G\t%5.03G\t%5.03G\n", nHoods[h], i, pR->tProc, pR->sr.Dt, pR->sr.sad, gM-pR->sr.sad, pR->sr.Dt / pR->tProc);
         pR++;
      }
   }
} // compareAnNHI


void scaleV3I (V3I *pR, const V3I *pS, const float s)
{
   pR->x= pS->x * s;
   pR->y= pS->y * s;
   pR->z= pS->z * s;
} // scaleV3I

int main (int argc, char *argv[])
{
   int r= 0;

   if (init(&gCtx,1<<8))
   {
      U8 nHoods[4]={6,14,18,26};
      MapInfo mi;
      float f=-1;

      if (argc > 1000)
      {
         const char *fileName= "s(256,256,256)u8.raw";//argv[1]
         f= mapFromU8Raw(gCtx.pM, &mi, &(gCtx.ws), fileName, 112, &(gCtx.org));
      }
      if (f <= 0)
      {
         f= setDefaultMap(gCtx.pM, &(gCtx.org.def), 1);
         scaleV3I(&(mi.m), &(gCtx.org.def), 0.5);
      }

      //pragma acc set device_type(acc_device_none) no effect ???

      //initW(gCtx.wPhase[0].w, 0.5, 6, 0); // ***M8***
      //iT= diffProcIsoD3S6M(gCtx.pSR[1], gCtx.pSR[0], &(gCtx.org), (D3S6IsoWeights*)(gCtx.wPhase), gCtx.pM, 100);

      if (0)//(1 == f)
      {
         compareAnNHI(nHoods, sizeof(nHoods), 20, 100);
      }
      else
      {
         TestParam param;
         RedRes     rrN;
         uint      iT, iN;

         param.nHood=6;
         param.iter= 100;
         param.rD=   0.5;
         initFieldVC(gCtx.pSR[0], &(gCtx.org), gM, &(mi.m));
         reduct3_2_0(&rrN, gCtx.ws.p, gCtx.pSR[0], &(gCtx.org));
         printf("smm: %G, %G, %G\n", rrN.sum, rrN.mm.vMin, rrN.mm.vMax );
         //dump(gCtx.pSR[0], &(mi.m), 2);

         initIsoW(gCtx.wPhase[0].w, 0.5, param.nHood, 0);
         iT= diffProcIsoD3SxM(gCtx.pSR[1], gCtx.pSR[0], &(gCtx.org), gCtx.wPhase, gCtx.pM, param.iter, param.nHood);
         iN= iT & 1;
         dump(gCtx.pSR[iN], gCtx.pM, &(mi.m), 2);
         reduct3_2_0(&rrN, gCtx.ws.p, gCtx.pSR[iN], &(gCtx.org));
         printf("smm: %G, %G, %G\n", rrN.sum, rrN.mm.vMin, rrN.mm.vMax );
         //dump(gCtx.pSR[0], &(mi.m), 2);
         save("rgb", "NRED.rgb", gCtx.ws.p, param.nHood, param.iter, -1, NULL);
      }
   }
   release(&gCtx);
   printf("Complete\n");
   return(r);
} // main
