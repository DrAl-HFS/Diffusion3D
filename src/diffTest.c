// diffTest.c - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan-June 2019

#include "diffTestUtil.h"

typedef struct
{
   DiffOrg        org;
   DiffScalar     *pSR[2];
   D3IsoWeights   wPhase[2];
   void           *pM, *pMC;
   MemBuff        ws;
} DiffTestContext;

//
typedef struct
{
   U16 def[5];
   U8  nDef, elemBytes;
} DataOrg;

typedef struct
{
   const char *pathName;
   const char *name;
   size_t      bytes;
} FileInfo;

typedef struct
{
   FileInfo file;
   DataOrg  org;
   int       maxIter;
} ArgInfo;

static DiffTestContext gCtx={0,};

static MapSiteInfo gMSI;

static ArgInfo gAI={0,};


/***/

U8 scanDef (U16 d[3], const char s[])
{
   int i= 0;
   U8 n=0;
   while (s[i] && !isdigit(s[i])) { ++i; }

   n+= sscanf(s+i,"%u",d+n);
   d[1]= d[2]= 0;
   return(n);
} // scanNameDef

Bool32 getOrg (DataOrg *pDO, const FileInfo *pFI, U8 verbose)
{
   pDO->nDef= scanDef(pDO->def, pFI->name);
   if (pDO->nDef > 0)
   {
      const U16 l= pDO->def[pDO->nDef-1];
      size_t x= pDO->def[0];
      for (U8 i=1; i < pDO->nDef; i++) { x*= pDO->def[i]; }

      while ((x < pFI->bytes) && (pDO->nDef < 3))
      {
         size_t d= pFI->bytes / x;
         //printf("%zu / %zu = %zu\n", pFI->bytes, x, d);
         if (d >= l)
         {
            pDO->def[ pDO->nDef++ ]= l;
            x*= l;
         }
      }
      // Pad to 3D
      while (pDO->nDef < 3) { pDO->def[ pDO->nDef++ ]= 1; }
      pDO->elemBytes= pFI->bytes / x;
      if (verbose)
      {
         printf("getOrg(%s) - [%u]=", pFI->name, pDO->nDef);
         for (U8 i=0; i < pDO->nDef; i++) { printf("%u ", pDO->def[i]); }
         printf("\n * %u = %zu (%zu)\n", pDO->elemBytes, x, pFI->bytes);
      }
      return(x <= pFI->bytes);
   }
   return(FALSE);
} // getOrg

void setDefaultDO (DataOrg *pO)
{
   if (0 == pO->nDef)
   {  
      pO->nDef= 3;
      pO->def[0]= pO->def[1]= pO->def[2]= 256;
      pO->elemBytes= 8;
   }
} // setDefaultDO

void scanArgs (ArgInfo *pAI, char *v[], const int n, U8 verbose)
{
   for (int i=0; i<n; i++)
   {
      char *pC= v[i];
      int a=-1;
      if ( isdigit(pC[0]) )
      {
         sscanf(pC,"%d",&a);
         if (a >= 0) { pAI->maxIter= a; }
      }
      else
      {
         size_t bytes= fileSize(pC);
         if (bytes > 0)
         {
            pAI->file.bytes= bytes;
            pAI->file.pathName= pC;
            pAI->file.name=     stripPath(pC);
            getOrg(&(pAI->org), &(pAI->file), verbose);
         }
      }
   }
   if (NULL == pAI->file.name) { setDefaultDO(&(pAI->org)); }
} // scanArgs

Bool32 init (DiffTestContext *pC, U16 def[3])
{
   initHack();
   if (initDiffOrg(&(pC->org), def, 1))
   {
      size_t b1M= pC->org.n1F * sizeof(D3MapElem); //*(pC->pM));
      size_t b1B= pC->org.n1B * sizeof(*(pC->pSR));
      size_t b1W= 8 * b1M; //pC->org.n1F * sizeof(*(pC->pSR));
      U32 nB= 0;

      pC->pM= malloc(b1M);
      pC->pMC= (D3S6MapElem*)(pC->pM) + pC->org.n1F; // NB only valid for M8
      pC->ws.bytes=  b1W; // 1<<28; ???
      pC->ws.p=  malloc(pC->ws.bytes);
      for (int i= 0; i<2; i++)
      {
         pC->pSR[i]= malloc(b1B);
         nB+= (NULL != pC->pSR[i]);
      }
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
   const char *fs, *eol;
} FmtDesc;

int dumpScalarRegion (char *pCh, int maxCh, const DiffScalar * pS, const MMV3I *pRegion, const Stride s[3])
{
   FmtDesc fd={"%.3G ","\n"};
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
         nCh+= snprintf(pCh+nCh, maxCh-nCh, fd.eol);
      }
      nCh+= snprintf(pCh+nCh, maxCh-nCh, fd.eol);
   }
   return(nCh);
} // dumpScalarRegion

int dumpMapRegion (char *pCh, int maxCh, const D3MapElem * pM, const MMV3I *pRegion, const Stride s[3])
{
   FmtDesc fd={"0x%x ","\n"};
   int nCh= 0;
   for (Index z= pRegion->vMin.z; z <= pRegion->vMax.z; z++)
   {
      for (Index y= pRegion->vMin.y; y <= pRegion->vMax.y; y++)
      {
         for (Index x= pRegion->vMin.x; x <= pRegion->vMax.x; x++)
         {
            const size_t i= x * s[0] + y * s[1] + z * s[2];
            nCh+= snprintf(pCh+nCh, maxCh-nCh, fd.fs, pM[i]);
         }
         nCh+= snprintf(pCh+nCh, maxCh-nCh, fd.eol);
      }
      nCh+= snprintf(pCh+nCh, maxCh-nCh, fd.eol);
   }
   return(nCh);
} // dumpMapRegion

void dumpSMR (const DiffScalar * pS, const D3MapElem * pM, const V3I *pC, int a)
{
   const MMV3I mm= {pC->x-a,pC->y-a,pC->z-a, pC->x+a,pC->y+a,pC->z+a};
   char buff[1<<14];
   const int m= sizeof(buff)-1;
   //adjustMMV3I(&mm, a);
   int n= dumpScalarRegion(buff, m, pS, &mm, gCtx.org.stride);

   if (pM)
   {
      Stride s[3]={ 1, gCtx.org.def.x, gCtx.org.def.x * gCtx.org.def.y };

      n+= dumpMapRegion(buff+n, m-n, pM, &mm, s);
   }
   if (n > 0) { printf("C(%d,%d,%d):-\n%s", pC->x, pC->y, pC->z, buff); }
} // dumpSMR

typedef struct
{
   U32 nHood, iter;
   DiffScalar  rD;
} TestParam;

typedef struct
{
   SMVal          tProc;
   SearchResult  sr;
} Test1Res;

//strExtFmtNSMV(" m=(",")")


U32 testAn (Test1Res *pR, const TestParam *pP)
{
   U32 iT=0, iN= 0, iA= 0;

   if (initIsoW(gCtx.wPhase[0].w, pP->rD, pP->nHood, 0) > 0)
   {
      initFieldVCM(gCtx.pSR[0], &(gCtx.org), NULL, NULL, &gMSI);
      deltaT();
      iT= diffProcIsoD3SxM(gCtx.pSR[1], gCtx.pSR[0], &(gCtx.org), gCtx.wPhase, gCtx.pM, pP->iter, pP->nHood);
      pR->tProc= deltaT();
      iN= iT & 1;

      // Estimate Diffusion-time moment
      AnResD3R2 a;
      analyseField(&a, gCtx.pSR[iN], &(gCtx.org));

      SMVal isovar= meanNSMV(a.var, 3);
      SMVal Dt= searchMin1(&(pR->sr), &(gCtx.ws), gCtx.pSR[iN], &(gCtx.org), gMSI.v, 0.5 * isovar, 0);

      char txt[256];
      const int m= sizeof(txt)-1;
      int n= 0;
      n= strFmtNSMV(txt, m, "%.4G,", a.mean, 3);
      printf("m=(%s)\n", txt);
      n= strFmtNSMV(txt, m, "%.4G,", a.var, 3);
      printf("v=(%s)\n", txt);
//m=(%.4G,%.4G,%.4G) v=(%.4G,%.4G,%.4G)
//a.m[0], a.m[1], a.m[2], a.v[0], a.v[1], a.v[2],
      printf("analyseField() - mm=%G,%G sum=%G isoVar=%G Dt=%G\n", a.mm.vMin, a.mm.vMax, a.sum, isovar, Dt);
   }
   return(iT);
} // testAn

void redCheck (MMSMVal *pMM, const DiffScalar *pN, const DiffScalar *pA, U32 f)
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

void save (const char *basePath, const char *baseName, const DiffScalar *pS, const U32 nHood, const U32 iter, const int z, const MMSMVal *pMM)
{
   char path[256];
   const int m= sizeof(path)-1;
   int n= 0;

   if (z >= 0)
   {
      n+= snprintf(path+n, m-n, "%s/N%02d_I%07d_Z%03d_%s", basePath, nHood, iter, z, baseName);
      saveSliceRGB(path, pS+dotS3(0,0,z,gCtx.org.stride), &(gCtx.org), pMM);
   }
   else
   {
      n+= snprintf(path+n, m-n, "%s/N%02d_I%07d_%s", basePath, nHood, iter, baseName);
      saveSliceRGB(path, pS, &(gCtx.org), pMM);
   }
} // save

void compareAnNHI (const U8 nHoods[], const U8 nNH, const U32 step, const U32 max)
{
   TestParam   param;
   Test1Res    res[4][5], *pR;
   U32        iT, iN, iA;

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
            initAnalytic(gCtx.pSR[iA], &(gCtx.org), gMSI.v, pR->sr.Dt);
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
         printf("%2u %3u\t%5.03G\t%5.03G\t%7.05G\t%5.03G\t%5.03G\n", nHoods[h], i, pR->tProc, pR->sr.Dt, pR->sr.sad, gMSI.v-pR->sr.sad, pR->sr.Dt / pR->tProc);
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

U32 testMap (RedRes *pRR, const TestParam * pTP, const MapDesc * pMD, U32 iT, const char *baseName)
{
   RedRes rrN;
   U32    iN=0, testV=0, dumpR=0;

   iN= iT & 1;
   initFieldVCM(gCtx.pSR[iN], &(gCtx.org), NULL, NULL, &gMSI);
   if (dumpR > 0) { dumpSMR(gCtx.pSR[iN], gCtx.pM, &(gMSI.c), dumpR); }

   reduct3_2_0(&rrN, gCtx.ws.p, gCtx.pSR[iN], &(gCtx.org), 0);
   printf("Initial SMM: %G, %G, %G\n", rrN.sum, rrN.mm.vMin, rrN.mm.vMax );
   // Set NAN / -1 for test
   if (testV > 0) { resetFieldVCM(gCtx.pSR[iN], &(gCtx.org), gCtx.pM, &(gDefObsKV.k), gDefObsKV.v2[1]); }

   initIsoW(gCtx.wPhase[0].w, 0.5, pTP->nHood, 0);
   switch (pMD->mapElemBytes)
   {
      case 1:
         // if (6 != pMD->nHood ) ERROR(...);
         iT+= diffProcIsoD3S6M8(gCtx.pSR[iN^1], gCtx.pSR[iN], &(gCtx.org), (D3S6IsoWeights*)gCtx.wPhase, gCtx.pM, pTP->iter);
         break;
      case 4:
         iT+= diffProcIsoD3SxM(gCtx.pSR[iN^1], gCtx.pSR[iN], &(gCtx.org), gCtx.wPhase, gCtx.pM, pTP->iter, pTP->nHood);
         break;
   }
   iN= iT & 1;

   // Clear NAN / -1 for tally
   if (testV > 0) { resetFieldVCM(gCtx.pSR[iN], &(gCtx.org), gCtx.pM, NULL, 0); }
   if (dumpR > 0) { dumpSMR(gCtx.pSR[iN], NULL, &(gMSI.c), dumpR); }

   const char mode= 0;
   reduct3_2_0(&rrN, gCtx.ws.p, gCtx.pSR[iN], &(gCtx.org), mode);
   printf("N%u I%u SMM: %G, %G, %G\n", pTP->nHood, iT, rrN.sum, rrN.mm.vMin, rrN.mm.vMax );
   if (pRR) { *pRR= rrN; }

   if (baseName)
   { // output
      MMSMVal mm, *pMM=NULL;
      if ('L' == mode)
      {
         mm.vMax= log(rrN.mm.vMax);
         if (rrN.mm.vMin > 0) { mm.vMin= log(rrN.mm.vMin); } else { mm.vMin= log(gEpsilon); }
         pMM= &mm;
      } else { pMM= &(rrN.mm); }

      save("rgb", baseName, gCtx.ws.p, pTP->nHood, pTP->iter, -1, pMM);
   }
   return(iT);
} // testMap

int main (int argc, char *argv[])
{
   MapDesc md={0};
   int r= 0;

   scanArgs(&gAI, argv+1, argc-1, TRUE);
   //utilTest();
   if (init(&gCtx, gAI.org.def))
   {
      float f=-1;
      U32 mapID= 0;
      gMSI.v= 1.0;

      if (gAI.file.bytes > 0)
      {
         RawTransMethodDesc rm={0};

         getProfileRM(&rm, TFR_ID_PDFPERM, MAP_ID_B1NH6, D3UF_NONE); // D3UF_PERM_SAVE|D3UF_CLUSTER_SAVE|D3UF_CLUSTER_TEST
         f= mapFromU8Raw(gCtx.pM, &md, &(gCtx.ws), gAI.file.pathName, &rm, &(gCtx.org));
         if (f > 0) { gMSI.c= md.site; }
         //printf("site= (%d,%d,%d)\n", gMSI.c.x, gMSI.c.y, gMSI.c.z);
         mapID= -1;
      }
      if (mapID <= 1)
      {
         f= setDefaultMap(gCtx.pM, &md, &(gCtx.org.def), mapID);
         scaleV3I(&(gMSI.c), &(gCtx.org.def), 0.5);
      }

      report(LOG0,"\nmain() - initialisation complete\n----\n");
      //pragma acc set device_type(acc_device_none) no effect ???

      //initW(gCtx.wPhase[0].w, 0.5, 6, 0); // ***M8***
      //iT= diffProcIsoD3S6M(gCtx.pSR[1], gCtx.pSR[0], &(gCtx.org), (D3S6IsoWeights*)(gCtx.wPhase), gCtx.pM, 100);

      if (0 == mapID) // && (4 == md.mapElemBytes))
      {
         static const U8 nHoods[]={6};//,14,18,26};
         compareAnNHI(nHoods, sizeof(nHoods), 20, 100);
      }
      else
      {
         TestParam  param;
         U32        iT=0, iN=0;
         RedRes    rr;

         //iT= 0; iN= iT & 1;
         param.nHood=md.nHood;
         param.iter= 50; //if (mapID > 0) { param.iter= 200; }
         param.rD=   0.5;
         iT= testMap(&rr, &param, &md, iT, "NRED.rgb");
         iN= iT & 1;
         printf("rr: s=%G um=%G mm=%G,%G\n", rr.sum, rr.uMin, rr.mm.vMin, rr.mm.vMax);

         // dependant diffusion test
         if (1 == md.mapElemBytes)
         {
            DiffScalar *pS= gCtx.pSR[iN];
            DupConsParam dcp={1E-15,1E-15};//lerpF64(rr.uMin, rr.mm.vMax, 1E-15);
            size_t n= map8DupCons(gCtx.pMC, gCtx.pM, &md, &(gCtx.org), pS, pS, &dcp);

            n= gCtx.org.n1F * sizeof(*pS);
            pS= malloc(n);
            if (pS)
            {
               memcpy(pS, gCtx.pSR[iN], n);
               SWAP(void*, gCtx.pMC, gCtx.pM);

               //param.iter= 200;
               iT= testMap(&rr, &param, &md, 0, "NDEP.rgb");
               iN= iT & 1;
               printf("rr: s=%G um=%G mm=%G,%G\n", rr.sum, rr.uMin, rr.mm.vMin, rr.mm.vMax);

               // swap back for cleanup
               SWAP(void*, gCtx.pMC, gCtx.pM);
               free(pS);
            }
         }
      }
   }
   release(&gCtx);
   report(LOG0,"Complete\n");
   return(r);
} // main
