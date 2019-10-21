// diff3D.h - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan-June 2019

#ifndef DIFF_3D_H
#define DIFF_3D_H

#include "mapUtil.h"

#ifndef DIFF_FUMEAN
#define DIFF_FUMEAN  // FUnctional MEasure ANalysis
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define DIFF_DIM (3)

typedef double DiffScalar;


// Buffer organisation (one or more planar/interleaved scalar fields)
typedef struct
{
   V3I      def; // TODO -> int def[3];
   int      nPhase;
   Stride   stride[DIFF_DIM], step[6];
   size_t   phaseStride;
   size_t   n1P,n1F,n1B; // number elements in single plane, field (phase) and single buffer (all phases)
} DiffOrg;

// Flags describing local structure
typedef U8 D3S6MapElem, D3S8MapElem;
typedef U16 D3S14MapElem;
typedef U32 D3MapElem;

// Isotropic weights
typedef struct
{
   DiffScalar w[2]; // For lattice distances 0, 1 OR sqrt(2) OR sqrt(3) (centre, face-neighbour OR edge-neighbour OR vertex-neighbour)
} D3S6IsoWeights;

// D3S8IsoWeights, D3S12IsoWeights, D3S20IsoWeights
// NB - diagonal-only stencils create a pair of distinct subspaces (black vs white chessboard)

typedef struct
{
   DiffScalar w[3]; // For lattice distances 0, 1, sqrt(2) OR sqrt(3) (centre, face-neighbour, edge-neighbour OR vertex-neighbour)
} D3S14IsoWeights, D3S18IsoWeights;

typedef struct
{
   DiffScalar w[4]; // For lattice distances 0, 1, sqrt(2), sqrt(3) (centre, face, edge and vertex neighbours)
} D3IsoWeights;


/***/

// Isotropic 3D diffusion functions:

#ifdef LDF3D
// Hacky exposure of lower level function for Fungi project integration & test
// (single shot, requires OpenACC buffer management beforehand i.e. no copy generated within)
extern void procD3S6M8
(
   DiffScalar * restrict pR,  // Result field(s)
   const DiffScalar  * const pS, // Source field(s)
   const DiffOrg     * const pO, // description
   const D3S6IsoWeights  * const pW,
   const D3S6MapElem * const pM8
);
#endif // LDF3D

// 6-point/2-weight stencil "3D Von-Neumann neighbourhood"
// NB 8bit map entries and only 2 weights
extern U32 diffProcIsoD3S6M8
(
   DiffScalar * restrict pR,  // Result field(s)
   DiffScalar * restrict pS,  // Source field(s)
   const DiffOrg     * pO, // descriptor
   const D3S6IsoWeights  * pW, // [pO->nPhase]
   const D3S6MapElem * pM8, // map (corresponding to scalar fields) describing structure
   const U32  nI   // iterations
);

// All stencils accessible through common interface
// NB 32bit map entries and all weights padded
U32 diffProcIsoD3SxM
(
   DiffScalar * restrict pR,  // Result field(s)
   DiffScalar * restrict pS,  // Source field(s)
   const DiffOrg       * pO, // descriptor
   const D3IsoWeights * pW,
   const D3MapElem    * pM,
   const U32 nI,
   const U32 nHood
);

// Expand 6 (-+strideXYZ) steps to 26 neighbourhood
extern void diffSet6To26 (Stride s26[]);

// Boundary check coordinates against the specified min-max volume and return flag mask of permissible memory accesses
extern U32 getBoundaryM26 (Index x, Index y, Index z, const MMV3I *pMM);
extern U32 getBoundaryM26V (Index x, Index y, Index z, const MMV3I *pMM); // verbose (debug) version

#ifdef  DIFF_FUMEAN  // DISPLACE to [diffFMA] ?
// Functional Measure Analysis data "packet"
typedef struct
{
   int i;   // iteration number
   float m[4]; // MKF measures
   float dt[2];
} FMAPkt;

extern Bool32 diffSetupFMA
(
   const int      maxSamples, 
   const char     relOpr[], 
   DiffScalar     t, 
   const DiffOrg *pO
);

extern int diffGetFMA (FMAPkt **ppAP, Bool32 reset);

#endif  // DIFF_FUMEAN

// Set analysis interval to power of two not exceeding parameter
// <0 -> disable
extern void diffSetIntervalFMA (int ivl);
extern void diffTeardownFMA (void);


extern void test (const DiffOrg * pO);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DIFF_3D_H
