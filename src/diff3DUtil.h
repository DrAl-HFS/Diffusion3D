// diff3DUtil.h - 3D Diffusion under OpenACC
// https://github.com/DrAl-HFS/Diffusion3D.git
// (c) Diffusion3D Project Contributors Jan 2019

#ifndef DIFF_3D_UTIL_H
#define DIFF_3D_UTIL_H

#include "diff3D.h"

#ifdef __cplusplus
extern "C" {
#endif

#define D3UF_  0
#define D3UF_PERM_SAVE     (1<<7)
#define D3UF_CLUSTER_SAVE  (1<<6)
#define D3UF_CLUSTER_TEST  (1<<5)
#define D3U_  ()

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
   U8 mapBytes, nHood, maxPermSet, permAlign;
   V3I site;
} MapDesc;


/***/

extern size_t dotS3 (Index x, Index y, Index z, const Stride s[3]);// { return( (size_t) (x*s[0]) + y*s[1] + z*s[2] ); }

extern void adjustMMV3I (MMV3I *pR, const MMV3I *pS, const I32 a);

extern size_t initDiffOrg (DiffOrg *pO, U32 def, U32 nP);

extern DiffScalar initIsoW (DiffScalar w[], DiffScalar r, U32 nHood, U32 f);

extern float setDefaultMap (D3MapElem *pM, MapDesc *pMD, const V3I *pD, const U32 id);

extern Bool32 getProfileRM (RawTransMethodDesc *pRM, const U8 idT, const U8 idM, const U8 f);

extern float mapFromU8Raw (void *pM, MapDesc *pMD, const MemBuff *pWS, const char *path, 
                              const RawTransMethodDesc *pRM, const DiffOrg *pO);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DIFF_3D_UTIL_H
