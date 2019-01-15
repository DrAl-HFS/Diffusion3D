// randMar.h - Gray-Scott Reaction-Diffusion using OpenACC
// https://github.com/DrAl-HFS/GSRD.git
// (c) GSRD Project Contributors Feb-October 2018

#ifndef DIFF_3D_H
#define DIFF_3D_H

#ifndef UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#ifndef MAX
#define MAX(a,b) (a)>(b)?(a):(b)
#endif

#endif

#define DIFF_DIM 3

typedef double DiffScalar;

typedef signed long  Index, Stride;

typedef struct { Index x, y, z; } V3I;
typedef struct { float x, y, z; } V3F;

// Buffer organisation (one or more planar/interleaved scalar fields)
typedef struct
{
   V3I      def;
   int      nPhase;
   Stride   stride[DIFF_DIM], step[DIFF_DIM*2];
   size_t   phaseStride;
   size_t   n1F,n1B; // number elements in single field (phase) and single buffer (all phases)
} DiffOrg;

typedef unsigned char D3S6MapElem; // Flags describing local structure

// Isotropic weights
typedef struct
{
   DiffScalar w[2]; // centre, neighbour
} D3S6IsoWeights;


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

#endif // DIFF_3D_H
