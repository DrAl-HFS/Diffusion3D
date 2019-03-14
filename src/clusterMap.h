// clusterMap.h - Map representation of cluster data

#ifndef CLUSTER_MAP_H
#define CLUSTER_MAP_H

#include "cluster.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CLF_VERBOSE  (1<<0)
#define CLF_REORDER1 (1<<1)
#define CLF_REORDER2 (1<<2)


extern void clusterMapTest (MemBuff ws, ClustIdx *pMaxNI, const size_t nNI, const U8 *pM, const MapOrg *pO);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // CLUSTER_MAP_H



