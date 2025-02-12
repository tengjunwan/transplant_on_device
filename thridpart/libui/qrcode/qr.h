#ifndef _QRXX_H_
#define _QRXX_H_

#include <inttypes.h>

int qr_get_image(const char* text, uint8_t** abgr, int* w, int* h, int zoom_in);
int GetQrImg(const char* text, uint8_t** abgr, int size, int border,
             uint32_t fg_color, uint32_t bg_color);

#endif
