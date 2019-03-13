// clusterMap.c - Map representation of cluster data

#include "clusterMap.h"




#include "cluster.h"

void analyseNH6 (const I32 * pNHNI, const size_t nNHNI)
{
   StatMomD1R2 s[6]={0}; // nH!
   StatResD1R2 r[6];
   U32 t, bth[32]={0};

   for (size_t i= 0; i < nNHNI; i+= 6)
   {
      U32 bt=0;
      for (U8 h=0; h < 6; h++)
      {
         I32 d= pNHNI[i+h];
         bt+= bitsReqI32(d);
         statMom1AddW(s+h, d, 1); 
      }
      bth[bt]++;
   }
   float sbth= 0;
   for (int i=0; i<32; i++) { sbth+= bth[i]; }
   float rN= 100.0 / sbth;
   printf("bth(%G):\n", sbth); sbth=0;
   for (int i=0; i<32; i++)
   {
      if (bth[i] > 0)
      {
         float x=1, v= bth[i] * rN;
         printf("%2d : %8.3G ", i, v);
         sbth+= v;
         while (v > x) { printf("*"); x+= 1; }
         printf("\n");
      }
   }
   printf("\n(sum=%G)\n", sbth);
   for (U8 h=0; h < 6; h++) { statMom1Res1(r+h, s+h, 0); printf("\n%G %G %G", s[h].m[0], s[h].m[1], s[h].m[2]); }
   printf("\nm: ");
   for (U8 h=0; h < 6; h++) { printf("%G ", r[h].m); }
   printf("\ns: ");
   for (U8 h=0; h < 6; h++) { printf("%G ", sqrt(r[h].v)); }
   printf("\n\n");
} // analyse

// NB - discards original ordering
I32 offsetArrange (U8 *pSignM, I32 t[6], const I32 o[6])
{
   U8 h, nz=0, signM=0;
   memcpy(t, o, 6*sizeof(I32));
   clusterSortAbsID(t, 6); // largest first: 654321

   while ((nz < 6) && (0 != t[nz])) { nz++; }
   for (h=0; (h+1) < nz; h+=2) SWAP(I32, t[h], t[h+1]); // pair swap: 563412

   for (h=0; h < nz; h++)
   {  // separate sign
      if (t[h] < 0) { t[h]= -t[h]; signM|= (1<<h); }
   }
   *pSignM= signM;

   for (h=0; (h+1) < nz; h+=2) { t[h+1]-= t[h]; } // pair delta: 5,(6-5),3,(4-3),1,(2-1)

   return(nz);
} // offsetArrange
 
typedef size_t NH6Pkt;
size_t compressNH6 (MemBuff *pWS, const I32 *pNHNI, const U32 nNI, U8 verbose)
{
   size_t bytes= nNI*sizeof(NH6Pkt);
   if (validBuff(pWS,bytes))
   {
      U8 signM; // 6 sign bits
      U8 brv[6]={0}, sbr, msbr=0;
      I32 d[6];
      U32 hn[128]={0};

      for (size_t i= 0; i < nNI; i++)
      {
         offsetArrange(&signM, d, pNHNI+i*6);
         sbr= 0;
         for (U8 h=0; h<6; h++)
         {
            U8 br= bitsReqI32(d[h]);
            sbr+= br;
            if (br > brv[h]) { brv[h]= br; }
         }
         if (verbose && (sbr >= msbr))
         {
            printf("%8zu: ", i);
            //for (h=0; h < 6; h++) { printf(" %+d", pNHNI[i*6+h]); }
            printf(" -> 0x%02x ", signM);
            for (U8 h=0; h < 6; h++) { printf(" %u", d[h]); }
            //printf(" ->");
            //for (h=0; h < 6; h++) { printf(" %u", d[h]); }
            printf(" (%u)\n", sbr);
         }
         msbr= MAX(msbr, sbr);
         hn[sbr]++;
      }
      printf("compressNH6() - brv[]=\n");
      for (U8 h=0; h < 6; h++) { printf(" %u", brv[h]); }
      if (verbose)
      {
         printf("\nhnb:\n");
         for (int i=0; i<128; i++)
         {
            if (hn[i] > 0)
            {
               printf("%3d: %8u\n", i, hn[i]);
            }
         }
      }

   }
   return(0);
} // compressNH6

void clusterMapTest (MemBuff ws, const ClustIdx *pMaxNI, const size_t nNI, const U8 *pM, const MapOrg *pO)
{
   size_t bytes= pO->n * sizeof(ClustIdx);
   if (ws.bytes >= bytes)
   {  // Build spatial map for neighbour lookup, then calculate offsets per site
      ClustIdx *pIdxMap= ws.p;
      const U8 nNH= 6;
      char ch[2];

      clusterOptimise(pMaxNI, nNI);
      printf("Building map... %p ", pIdxMap);
      memset(ws.p, 0, bytes);
      adjustBuff(&ws, &ws, bytes, 0);
      for (size_t i= 0; i < nNI; i++) { ClustIdx j= pMaxNI[i]; pIdxMap[ j ]= i; }
      printf("%G%cbytes\n", binSizeZ(ch,bytes), ch[0]);
      bytes= nNI * nNH * sizeof(ClustIdx);
      if (ws.bytes >= bytes)
      {
         I32 *pNHNI= ws.p;
         U32 nNHNI= nNH * nNI, err=0,c=0;
         printf("Computing offsets... %p ", pNHNI);
         memset(ws.p, 0, bytes);
         adjustBuff(&ws,&ws,bytes,0);
         for (size_t i= 0; i < nNI; i++)
         {
            const size_t k= i * nNH;
            const ClustIdx j= pMaxNI[i];
            const U8 m= pM[j];

            err+= (i != pIdxMap[j]);
            for (U8 h=0; h<nNH; h++)
            {
               if (m & (1<<h)) { pNHNI[k+h]= pIdxMap[ j + pO->step[h] ] - i; }
            }
         }
         printf("%G%cbytes\n", binSizeZ(ch,bytes), ch[0]);
         if (err > 0) { printf("ERROR: index map corrupt (%u)\n", err); }

         printf("Analysing...\n");
         //analyseNH6(pNHNI, nNHNI);
         compressNH6(&ws,pNHNI,nNI,0);

      } else printf("ERROR: offsetMapTest() - only %G%cbytes of %G%cbytes avail\n", binSizeZ(ch+0,ws.bytes), ch[0], binSizeZ(ch+1,bytes), ch[1]);
   }
} // clusterMapTest
