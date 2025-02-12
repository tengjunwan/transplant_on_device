/*
  Copyright (c), 2001-2022, Shenshu Tech. Co., Ltd.
 */

#ifndef SAMPLE_NPU_PROCESS_H
#define SAMPLE_NPU_PROCESS_H

#include "ot_type.h"
#include "detectobjs.h"

td_void nnn_init(td_void);
td_s32 nnn_execute(td_void* data_buf, unsigned long data_len,stYolovDetectObjs* pOut);
td_void nnn_feature_init(td_void);
#endif
