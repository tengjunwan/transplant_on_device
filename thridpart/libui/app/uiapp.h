#ifndef _UIAPP_MAIN_H_
#define _UIAPP_MAIN_H_

#include "detectobjs.h"

#if __cplusplus
extern "C" {
#endif
int UiAppMain();
int UiAppMainStop();
void CreateUi();
void UiExit();
void DrawOsdInfo(stYolovDetectObjs *pOut);
#if __cplusplus
}
#endif
#endif
