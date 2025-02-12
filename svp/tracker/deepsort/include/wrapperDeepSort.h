
#ifndef _WRAPPERDEEPSORT_H__
#define _WRAPPERDEEPSORT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "detectobjs.h" // ho+

int deepsort_init();
// int bytetrack_update(std::vector<detect_result>& results);
void track_deepsort(stYolovDetectObjs *pOut);

#ifdef __cplusplus
}
#endif
#endif