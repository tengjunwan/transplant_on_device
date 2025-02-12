#ifndef _WRAPPERNCNN_H__
#define _WRAPPERNCNN_H__
#include "detectobjs.h"
#ifdef __cplusplus
extern "C" {
#endif

int ncnn_result(float *src,unsigned int len,stYolovDetectObjs* pOut);

int ncnn_result_yolov8(float *src,unsigned int len,stYolovDetectObjs* pOut);

#ifdef __cplusplus
}
#endif
#endif