// diff3D.h - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan 2019

#ifndef DIFF_3D_H
#define DIFF_3D_H

#include "util.h"


#define DIFF_DIM (3)

typedef double DiffScalar;

typedef signed long  Index, Stride;

typedef struct { Index x, y, z; } V3I;
//typedef struct { float x, y, z; } V3F;

// Buffer organisation (one or more planar/interleaved scalar fields)
typedef struct
{
   V3I      def;
   int      nPhase;
   Stride   stride[DIFF_DIM], step[6];
   size_t   phaseStride;
   size_t   n1P,n1F,n1B; // number elements in single plane, field (phase) and single buffer (all phases)
} DiffOrg;

// Flags describing local structure
typedef unsigned char D3S6MapElem, D3S8MapElem;
typedef ushort D3S14MapElem;
typedef uint D3MapElem;

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

// 6-point/2-weight stencil "3D Von-Neumann neighbourhood"
// NB 8bit map entries & no weight padding
extern uint diffProcIsoD3S6M 
(
   DiffScalar * restrict pR,  // Result field(s)
   DiffScalar * restrict pS,  // Source field(s)
   const DiffOrg * pO,        // descriptor
   const D3S6IsoWeights * pW, // [pO->nPhase]
   const D3S6MapElem *pM,     // map (corresponding to scalar fields) describing structure
   const uint  nI   // iterations
);

// Other stencils accessed through common interface
// NB 32bit map entries and all weights padded 
uint diffProcIsoD3SxM
(
   DiffScalar * restrict pR,  // Result field(s)
   DiffScalar * restrict pS,  // Source field(s)
   const DiffOrg        * pO, // descriptor
   const D3IsoWeights * pW,
   const D3MapElem    * pM,
   const uint nI,
   const uint nHood
);

extern void diffSet6To26 (Stride s26[]);

extern void test (const DiffOrg * pO);

#endif // DIFF_3D_H
