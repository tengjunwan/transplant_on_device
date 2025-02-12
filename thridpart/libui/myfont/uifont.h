/*
 * Copyright 2021 NXP
 * SPDX-License-Identifier: MIT
 */

#ifndef UIFONT_H
#define UIFONT_H
#ifdef __cplusplus
extern "C" {
#endif

#include "./freetype2/lv_freetype.h"
#include "lvgl/lvgl.h"

void init_font_size();
lv_font_t *get_font_simsun(int size);
lv_font_t *get_font_opposan(int size);

// static lv_img_dsc_t start_src;
extern lv_ft_info_t font_simsun_14;
extern lv_ft_info_t font_simsun_16;
extern lv_ft_info_t font_simsun_18;
extern lv_ft_info_t font_simsun_22;
extern lv_ft_info_t font_simsun_20;
extern lv_ft_info_t font_simsun_24;
extern lv_ft_info_t font_simsun_28;
extern lv_ft_info_t font_simsun_30;
extern lv_ft_info_t font_simsun_34;
extern lv_ft_info_t font_simsun_48;
extern lv_ft_info_t font_simsun_58;
extern lv_ft_info_t font_simsun_72;

extern lv_ft_info_t font_OPPOSansB_12;
extern lv_ft_info_t font_OPPOSansB_16;
extern lv_ft_info_t font_OPPOSansB_18;
extern lv_ft_info_t font_OPPOSansB_24;
extern lv_ft_info_t font_OPPOSansB_28;
extern lv_ft_info_t font_OPPOSansB_30;
extern lv_ft_info_t font_OPPOSansB_34;
extern lv_ft_info_t font_OPPOSansB_36;
extern lv_ft_info_t font_OPPOSansB_48;
extern lv_ft_info_t font_OPPOSansB_58;
extern lv_ft_info_t font_OPPOSansB_60;
extern lv_ft_info_t font_OPPOSansB_72;

// bool ReadImage(char *path,lv_img_dsc_t *img_src);
#ifdef __cplusplus
}
#endif
#endif