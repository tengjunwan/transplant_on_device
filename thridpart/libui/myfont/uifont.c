/*
 * Copyright 2021 NXP
 * SPDX-License-Identifier: MIT
 */

#include "./uifont.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lvgl/lvgl.h"

static void init_font_bold_size();

lv_ft_info_t font_simsun_14;
lv_ft_info_t font_simsun_16;
lv_ft_info_t font_simsun_18;
lv_ft_info_t font_simsun_20;
lv_ft_info_t font_simsun_22;
lv_ft_info_t font_simsun_24;
lv_ft_info_t font_simsun_28;
lv_ft_info_t font_simsun_30;
lv_ft_info_t font_simsun_34;
lv_ft_info_t font_simsun_48;
lv_ft_info_t font_simsun_58;
lv_ft_info_t font_simsun_72;

void init_font_size() {
    font_simsun_14.name = "/app/exec/DroidSans.ttf";
    font_simsun_14.weight = 14;
    font_simsun_14.style = FT_FONT_STYLE_NORMAL;
    lv_ft_font_init(&font_simsun_14);

    font_simsun_16.name = "/app/exec/DroidSans.ttf";
    font_simsun_16.weight = 16;
    font_simsun_16.style = FT_FONT_STYLE_NORMAL;
    lv_ft_font_init(&font_simsun_16);

    font_simsun_18.name = "/app/exec/DroidSans.ttf";
    font_simsun_18.weight = 18;
    font_simsun_18.style = FT_FONT_STYLE_NORMAL;
    lv_ft_font_init(&font_simsun_18);

    font_simsun_20.name = "/app/exec/DroidSans.ttf";
    font_simsun_20.weight = 20;
    font_simsun_20.style = FT_FONT_STYLE_NORMAL;
    lv_ft_font_init(&font_simsun_20);

    font_simsun_22.name = "/app/exec/DroidSans.ttf";
    font_simsun_22.weight = 22;
    font_simsun_22.style = FT_FONT_STYLE_NORMAL;
    lv_ft_font_init(&font_simsun_22);

    font_simsun_24.name = "/app/exec/DroidSans.ttf";
    font_simsun_24.weight = 24;
    font_simsun_24.style = FT_FONT_STYLE_NORMAL;
    lv_ft_font_init(&font_simsun_24);

    font_simsun_28.name = "/app/exec/DroidSans.ttf";
    font_simsun_28.weight = 28;
    font_simsun_28.style = FT_FONT_STYLE_NORMAL;
    lv_ft_font_init(&font_simsun_28);

    font_simsun_30.name = "/app/exec/DroidSans.ttf";
    font_simsun_30.weight = 30;
    font_simsun_30.style = FT_FONT_STYLE_NORMAL;
    lv_ft_font_init(&font_simsun_30);

    font_simsun_34.name = "/app/exec/DroidSans.ttf";
    font_simsun_34.weight = 34;
    font_simsun_34.style = FT_FONT_STYLE_NORMAL;
    lv_ft_font_init(&font_simsun_34);

    font_simsun_48.name = "/app/exec/DroidSans.ttf";
    font_simsun_48.weight = 48;
    font_simsun_48.style = FT_FONT_STYLE_NORMAL;
    lv_ft_font_init(&font_simsun_48);

    font_simsun_58.name = "/app/exec/DroidSans.ttf";
    font_simsun_58.weight = 58;
    font_simsun_58.style = FT_FONT_STYLE_NORMAL;
    lv_ft_font_init(&font_simsun_58);

    font_simsun_72.name = "/app/exec/DroidSans.ttf";
    font_simsun_72.weight = 72;
    font_simsun_72.style = FT_FONT_STYLE_NORMAL;
    lv_ft_font_init(&font_simsun_72);

    init_font_bold_size();
}

lv_font_t *get_font_simsun(int size) {
    if (size >= 72) {
        return font_simsun_72.font;
    } else if (size >= 58) {
        return font_simsun_58.font;
    } else if (size >= 48) {
        return font_simsun_48.font;
    } else if (size >= 34) {
        return font_simsun_34.font;
    } else if (size >= 30) {
        return font_simsun_30.font;
    } else if (size >= 28) {
        return font_simsun_28.font;
    } else if (size >= 24) {
        return font_simsun_24.font;
    } else if (size >= 22) {
        return font_simsun_22.font;
    } else if (size >= 20) {
        return font_simsun_20.font;
    } else if (size >= 18) {
        return font_simsun_18.font;
    } else if (size >= 16) {
        return font_simsun_16.font;
    } else if (size >= 14) {
        return font_simsun_14.font;
    }
    return font_simsun_14.font;
}

lv_ft_info_t font_OPPOSansB_12;
lv_ft_info_t font_OPPOSansB_16;
lv_ft_info_t font_OPPOSansB_18;
lv_ft_info_t font_OPPOSansB_24;
lv_ft_info_t font_OPPOSansB_28;
lv_ft_info_t font_OPPOSansB_30;
lv_ft_info_t font_OPPOSansB_34;
lv_ft_info_t font_OPPOSansB_36;
lv_ft_info_t font_OPPOSansB_48;
lv_ft_info_t font_OPPOSansB_58;
lv_ft_info_t font_OPPOSansB_60;
lv_ft_info_t font_OPPOSansB_72;

static void init_font_bold_size() {
    font_OPPOSansB_12.name = "/app/exec/OPPOSans-B-2.ttf";
    font_OPPOSansB_12.weight = 12;
    font_OPPOSansB_12.style = FT_FONT_STYLE_BOLD;
    lv_ft_font_init(&font_OPPOSansB_12);

    font_OPPOSansB_16.name = "/app/exec/OPPOSans-B-2.ttf";
    font_OPPOSansB_16.weight = 16;
    font_OPPOSansB_16.style = FT_FONT_STYLE_BOLD;
    lv_ft_font_init(&font_OPPOSansB_16);

    font_OPPOSansB_18.name = "/app/exec/OPPOSans-B-2.ttf";
    font_OPPOSansB_18.weight = 18;
    font_OPPOSansB_18.style = FT_FONT_STYLE_BOLD;
    lv_ft_font_init(&font_OPPOSansB_18);

    font_OPPOSansB_24.name = "/app/exec/OPPOSans-B-2.ttf";
    font_OPPOSansB_24.weight = 24;
    font_OPPOSansB_24.style = FT_FONT_STYLE_BOLD;
    lv_ft_font_init(&font_OPPOSansB_24);

    font_OPPOSansB_28.name = "/app/exec/OPPOSans-B-2.ttf";
    font_OPPOSansB_28.weight = 28;
    font_OPPOSansB_28.style = FT_FONT_STYLE_BOLD;
    lv_ft_font_init(&font_OPPOSansB_28);

    font_OPPOSansB_30.name = "/app/exec/OPPOSans-B-2.ttf";
    font_OPPOSansB_30.weight = 30;
    font_OPPOSansB_30.style = FT_FONT_STYLE_BOLD;
    lv_ft_font_init(&font_OPPOSansB_30);

    font_OPPOSansB_34.name = "/app/exec/OPPOSans-B-2.ttf";
    font_OPPOSansB_34.weight = 34;
    font_OPPOSansB_34.style = FT_FONT_STYLE_BOLD;
    lv_ft_font_init(&font_OPPOSansB_34);

    font_OPPOSansB_36.name = "/app/exec/OPPOSans-B-2.ttf";
    font_OPPOSansB_36.weight = 36;
    font_OPPOSansB_36.style = FT_FONT_STYLE_BOLD;
    lv_ft_font_init(&font_OPPOSansB_36);

    font_OPPOSansB_48.name = "/app/exec/OPPOSans-B-2.ttf";
    font_OPPOSansB_48.weight = 48;
    font_OPPOSansB_48.style = FT_FONT_STYLE_BOLD;
    lv_ft_font_init(&font_OPPOSansB_48);

    font_OPPOSansB_58.name = "/app/exec/OPPOSans-B-2.ttf";
    font_OPPOSansB_58.weight = 58;
    font_OPPOSansB_58.style = FT_FONT_STYLE_BOLD;
    lv_ft_font_init(&font_OPPOSansB_58);

    font_OPPOSansB_60.name = "/app/exec/OPPOSans-B-2.ttf";
    font_OPPOSansB_60.weight = 60;
    font_OPPOSansB_60.style = FT_FONT_STYLE_BOLD;
    lv_ft_font_init(&font_OPPOSansB_60);

    font_OPPOSansB_72.name = "/app/exec/OPPOSans-B-2.ttf";
    font_OPPOSansB_72.weight = 72;
    font_OPPOSansB_72.style = FT_FONT_STYLE_BOLD;
    lv_ft_font_init(&font_OPPOSansB_72);
}

lv_font_t *get_font_opposan(int size) {
    if (size >= 72) {
        return font_OPPOSansB_72.font;
    } else if (size >= 60) {
        return font_OPPOSansB_60.font;
    } else if (size >= 58) {
        return font_OPPOSansB_58.font;
    } else if (size >= 48) {
        return font_OPPOSansB_48.font;
    } else if (size >= 36) {
        return font_OPPOSansB_36.font;
    } else if (size >= 34) {
        return font_OPPOSansB_34.font;
    } else if (size >= 30) {
        return font_OPPOSansB_30.font;
    } else if (size >= 28) {
        return font_OPPOSansB_28.font;
    } else if (size >= 24) {
        return font_OPPOSansB_24.font;
    } else if (size >= 18) {
        return font_OPPOSansB_18.font;
    } else if (size >= 16) {
        return font_OPPOSansB_16.font;
    }
    return font_OPPOSansB_12.font;
}