// diff3DUtil.h - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan-June 2019

#ifndef DIFF_3D_UTIL_H
#define DIFF_3D_UTIL_H

#include "diff3D.h"

#ifdef __cplusplus
extern "C" {
#endif

#define D3UF_NONE          0
#define D3UF_PERM_SAVE     (1<<7)
#define D3UF_CLUSTER_SAVE  (1<<6)
#define D3UF_CLUSTER_TEST  (1<<5)
#define D3UF_ALL  (-1)

#define MAP_ID_B1NH6    (0)
#define MAP_ID_B4NH6    (1)
#define MAP_ID_B4NH18   (2)
#define MAP_ID_B4NH26   (3)

#define TFR_ID_RAW       (0)
#define TFR_ID_THRESHOLD (1)
#define TFR_ID_PDFPERM   (2)


typedef struct { V3I c; float v; } MapSiteInfo;

typedef struct
{
   D3MapElem m, v;   // Mask and compare value
} D3MapKey;

typedef struct
{
   U8 method, nBNH, maxPermLvl, flags;
   float param[2];
} RawTransMethodDesc;

typedef struct
{
   U8 mapElemBytes, nHood, maxPermSet, permAlign;
   V3I site;
   char msg[256];
} MapDesc;

typedef struct
{
   DiffScalar t[2]; // Hysteresis threshold: <= t[0], >= t[1]
   U8 flags, logLvl, pad[6];
} DupConsParam;


/***/

extern size_t initDiffOrg (DiffOrg *pO, const U16 def[3], U32 nP);

extern DiffScalar initIsoW (DiffScalar w[], DiffScalar r, U32 nHood, U32 f);

extern float setDefaultMap (D3MapElem *pM, MapDesc *pMD, const V3I *pD, const U32 id);

extern Bool32 getProfileRM (RawTransMethodDesc *pRM, const U8 idT, const U8 idM, const U8 f);

extern float mapFromU8Raw (void *pM, MapDesc *pMD, const MemBuff *pWS, const char *path, 
                              const RawTransMethodDesc *pRM, const DiffOrg *pO);

// Hacky constraint for M8 only
//extern size_t constrainMap (void * pM, const void * pW, const MapDesc * pMD, const DiffOrg *pO);
extern size_t map8DupCons
(
   D3S6MapElem    * pR,     // result constrained map
   const D3S6MapElem * pM, // reference map
   const MapDesc  * pMD,    // map props
   const DiffOrg  * pO,     // scalar props
   const DiffScalar * pS0,  // scalar field
   const DiffScalar * pS1,  // scalar field
   const DupConsParam * pP      // threshold >= 
);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DIFF_3D_UTIL_H
