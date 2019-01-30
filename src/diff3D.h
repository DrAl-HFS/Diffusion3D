// diff3D.h - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan 2019

#ifndef DIFF_3D_H
#define DIFF_3D_H

#include "util.h"


#define DIFF_DIM (3)
#define DIFF_DIR (DIFF_DIM*2)
//define DIFF_W (2)

typedef double DiffScalar;

typedef signed long  Index, Stride;

typedef struct { Index x, y, z; } V3I;
//typedef struct { float x, y, z; } V3F;

// Buffer organisation (one or more planar/interleaved scalar fields)
typedef struct
{
   V3I      def;
   int      nPhase;
   Stride   stride[DIFF_DIM], step[DIFF_DIR];
   size_t   phaseStride;
   size_t   n1F,n1B; // number elements in single field (phase) and single buffer (all phases)
} DiffOrg;

// Flags describing local structure
typedef unsigned char D3S6MapElem;
typedef unsigned short D3S14MapElem;
typedef unsigned int D3S18MapElem;
typedef unsigned int D3S26MapElem;

// Isotropic weights
typedef struct
{
   DiffScalar w[2]; // For lattice distances 0, 1 (centre, face-neighbour)
} D3S6IsoWeights;

typedef struct
{
   DiffScalar w[3]; // For lattice distances 0, 1, sqrt(3) (centre, face-neighbour, vertex-neighbour)
} D3S14IsoWeights;

typedef struct
{
   DiffScalar w[3]; // For lattice distances 0, 1, sqrt(2) (centre, face-neighbour, edge-neighbour)
} D3S18IsoWeights;

typedef struct
{
   DiffScalar w[4]; // For lattice distances 0, 1, sqrt(2), sqrt(3) (centre, face, edge and vertex neighbours)
} D3S26IsoWeights;


/***/

// Isotropic 3D 6-point/2-weight stencil "3D Von-Neumann neighbourhood"
extern uint diffProcIsoD3S6M 
(
   DiffScalar * restrict pR,  // Result field(s)
   DiffScalar * restrict pS,  // Source field(s)
   const DiffOrg * pO,        // descriptor
   const D3S6IsoWeights * pW, // [pO->nPhase]
   const D3S6MapElem *pM,     // map (corresponding to scalar fields) describing structure
   const uint  nI   // iterations
);

extern uint diffProcIsoD3S14M
(
   DiffScalar * restrict pR,  // Result field(s)
   DiffScalar * restrict pS, // Source field(s)
   const DiffOrg        * pO, // descriptor
   const D3S14IsoWeights * pW,
   const D3S14MapElem    * pM,
   const uint nI
);

#endif // DIFF_3D_H
