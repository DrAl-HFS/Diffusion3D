#include "cluster.h"

//typedef signed long Offset;

size_t findNIMinD (const ClustIdx u[], const size_t n, const ClustIdx v)
{
   size_t iM=0;
   ClustIdx m= abs(u[0] - v);
   for (size_t i=1; i<n; i++)
   {
      U32 d= abs(u[i] - v);
      if (d < m) { m= d; iM= i; }
   }
   return(iM);
} // findNIMinD

Offset index3 (I32 x, I32 y, I32 z, const Stride stride[3]) { return(x * stride[0] + y * stride[1] + z * stride[2]); }
void split3 (I32 v[3], ClustIdx o, const Stride stride[3])
{
#ifdef GNU
   ldiv_t d= div(o, stride[2]);
   v[2]= d.quot;
   d= div(d.rem, stride[1]);
   v[2]= d.quot;
   v[1]= d.rem;
#else
   v[2]= o / stride[2]; o= o % stride[2];
   v[1]= o / stride[1]; o= o % stride[1];
   // if (stride[0] > 1) { v[0]= o / stride[0]; } else 
   v[0]= o;
#endif
} // split3

// Hybrid X-depth-first / Y&Z-breadth-first search to improve index locality 
static size_t clusterXDYZBN6 (ClustIdx *pNI, U32 *pNIC, U8 *pU8, const V3I *pDef, const Stride stride[3])
{
   const I32 min[3]= {0,0,0};
   const I32 max[3]= { pDef->x-1, pDef->y-1, pDef->z-1 };
   //const I32 stride[3]= {1,def[0],def[0]*def[1]};
   size_t nI=0, nC=0, j=0, nJ=0;
   I32 v[3];
   U8 l= 2;

   nI= stride[2] * pDef->z;
   pNIC[0]= 0; // nul entry
   for (size_t i= 0; i<nI; i++)
   {
      if (1 == pU8[i])
      {
         pNI[nJ++]= i; // added temporarily for DFS
         pU8[i]= l;
         do
         {
            ClustIdx t, k= pNI[j++];

            split3(v, k, stride);
            
#if 1       // X-axis LR-ordered DFS (but with break at initial point dictated by parent k= pNI[j] )
            I32 m, n;
            m= v[0] - min[0]; n=0; t= k;
            while ((m-- > 0) && (1 == pU8[t - stride[0]])) { t-= stride[0]; n++; } // pNI[nJ++]= t;
            for (m=0; m<n; m++) { pNI[nJ++]= t; pU8[t]= l; t+= stride[0]; }
            m= max[0] - v[0]; t= k;
            while ((m-- > 0) && (1 == pU8[t + stride[0]])) { t+= stride[0]; pNI[nJ++]= t; pU8[t]= l; }
#else       // X BFS
            if ((v[0] > min[0]) && (1 == pU8[k - stride[0]])) { t= k - stride[0]; pNI[nJ++]= t; pU8[t]= l; }
            if ((v[0] < max[0]) && (1 == pU8[k + stride[0]])) { t= k + stride[0]; pNI[nJ++]= t; pU8[t]= l; }
#endif
            // Y BFS
            if ((v[1] > min[1]) && (1 == pU8[k - stride[1]])) { t= k - stride[1]; pNI[nJ++]= t; pU8[t]= l; }
            if ((v[1] < max[1]) && (1 == pU8[k + stride[1]])) { t= k + stride[1]; pNI[nJ++]= t; pU8[t]= l; }

            // Z BFS
            if ((v[2] > min[2]) && (1 == pU8[k - stride[2]])) { t= k - stride[2]; pNI[nJ++]= t; pU8[t]= l; }
            if ((v[2] < max[2]) && (1 == pU8[k + stride[2]])) { t= k + stride[2]; pNI[nJ++]= t; pU8[t]= l; }

         } while (j < nJ);
         l+= (l < 255);
         pNIC[nC++]= nJ;
      }
   }
   pNIC[nC]= nI; // Termination
   return(nC);
} // clusterXDYZBN6

U32 clusterExtract (ClustRes *pR, const MemBuff *pWS, U8 *pImg, const V3I *pDef, const Stride stride[3])
{
   const size_t n= pDef->x * pDef->y * pDef->z;
   size_t mN, mC;
   ClustRes r;

   U32 m= pWS->bytes / sizeof(ClustIdx);
   mN= n;
   mC= 0.5 * mN + 2;
   while ((mN+mC) > m)
   { 
      float f= (m / (mN+mC));
      mN*= f;
      mC*= f;
   }

   r.pNC= pWS->p;
   
   r.pNI= r.pNC+mC; mN-= 1;
   printf("Cluster prep max: %unodes %uclusters\n", mN, mC);
   r.nNC= clusterXDYZBN6(r.pNI, r.pNC, pImg, pDef, stride);
   r.nNI= r.pNC[r.nNC-1];
   printf("Found: %unodes %uclusters\n", r.nNI, r.nNC);

   r.iNCMax= 0;
   m= 1000;
   mC= 0;

   for (U32 i=1; i < r.nNC; i++)
   {
      U32 s= r.pNC[i] - r.pNC[i-1];
      if (s >= m)
      {
         printf("C%u : %u\n", i, s);
         if (s > mC) { r.iNCMax= i; mC= s; }
      }
   }
   if (pR) { *pR= r; }
   return(r.nNC);
} // clusterExtract

U32 clusterResGetMM (MMU32 *pMM, const ClustRes *pCR, U32 iC)
{
   if ((iC > 0) && (iC < pCR->nNC))
   {
      pMM->vMin= pCR->pNC[iC-1];
      pMM->vMax= pCR->pNC[iC];
      
      return(pMM->vMax - pMM->vMin);
   }
   //else
   pMM->vMin= pMM->vMax= 0;
   return(0);
} // clusterResGetMM

int cmpU32A (U32 *a, U32 *b) { return(*a - *b); } // ascending
void clusterSortUA (U32 u[], U32 n) { qsort(u, n, sizeof(U32), cmpU32A); }

int cmpAbsI32D (I32 *a, I32 *b) { return(abs(*b) - abs(*a)); } // descending
void clusterSortAbsID (I32 i[], U32 n) { qsort(i, n, sizeof(I32), cmpAbsI32D); }

#define CLUST_SEQ_MAX 63
void clusterOptimise (ClustIdx ni[], const size_t nNI)
{
   U32 s=0, c=0, nc=0, hc[CLUST_SEQ_MAX+1]={0,}, hnc[CLUST_SEQ_MAX+1]={0,};
   for (size_t i=1; i<nNI; i++)
   {
      int d= ni[i] - ni[i-1];
      if (1 == abs(d))
      {
         c++;
         if (-1 == d) { SWAP(ClustIdx, ni[i], ni[i-1]); s++; }
         if (nc > 0) { hnc[MIN(CLUST_SEQ_MAX,nc-1)]++; nc= 0; }
      }
      else
      {
         nc++;
         if (c > 0) { hnc[MIN(CLUST_SEQ_MAX,c-1)]++; c= 0; }
      }
   }
   if (nc > 0) { hnc[MIN(CLUST_SEQ_MAX,nc-1)]++; nc= 0; }
   if (c > 0) { hnc[MIN(CLUST_SEQ_MAX,c-1)]++; c= 0; }

   printf("clusterOptimise() - %u swaps\n", s);
   for (int i= 0; i<CLUST_SEQ_MAX; i++)
   {
      if (hc[i] > 0)
      {
         printf("%d: %8u %8u\n", i+1, hc[i], hnc[i]);
      }
   }
} // clusterOptimise

#ifdef CLUSTER_DEBUG

void testC (Stride s[3])
{
   I32 v[3], d=100;
   printf("test(%d,%d,%d)\n", s[0], s[1], s[2]);
   for (size_t i= 0; i<(1<<20); i+= d)
   {
      split3(v, i, s);
      size_t j= index3(v[0], v[1], v[2], s);
      printf("%zu -> (%u,%u,%u) -> %zu\n", i, v[0], v[1], v[2], j);
      d+= 100;
   }
} // testCS

int main (int argc, char *argv[])
{
   return(0);
} // main

#endif // CLUSTER_DEBUG

