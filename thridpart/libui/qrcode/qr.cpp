#include "qr.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>

#include "qrencode.h"
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"

// Prescaler (number of pixels in bmp file for each QRCode pixel, on each
#define COLOR_DEPTH32
#ifdef COLOR_DEPTH32
#define PIXEL_COLOR_R 0x00
#define PIXEL_COLOR_G 0x00
#define PIXEL_COLOR_B 0x0
#define PIXELSIZE 4
const char POINTDATA[4] = {PIXEL_COLOR_B, PIXEL_COLOR_G, PIXEL_COLOR_R, 0xFF};
#endif

#ifdef COLOR_DEPTH16
#define PIXELSIZE 2
const char POINTDATA[4] = {0x00, 0x00};
#endif

int qr_get_image(const char* text, uint8_t** abgr, int* w, int* h,
                 int zoom_in) {
    if (text == NULL || abgr == NULL || w == NULL || h == NULL) return -1;

    *abgr = NULL;

    unsigned int unWidth, x, y, l, n, unWidthAdjusted, unDataBytes;
    unsigned char *pSourceData, *pDestData;
    QRcode* pQRC;

    // Compute QRCode
    if (pQRC = QRcode_encodeString(text, 0, QR_ECLEVEL_H, QR_MODE_8, 1)) {
        //矩阵的维数
        unWidth = pQRC->width;
        unWidthAdjusted = unWidth * zoom_in * PIXELSIZE;
        unDataBytes = unWidthAdjusted * unWidth * zoom_in;

        *abgr = (uint8_t*)malloc(unDataBytes);
        if (*abgr == NULL) {
            printf("Out of memory");
            QRcode_free(pQRC);
            return -1;
        }
        if (w != NULL) {
            *w = unWidth * zoom_in;
        }
        if (h != NULL) {
            *h = unWidth * zoom_in;
        }

        // Convert QrCode bits to bmp pixels
        pSourceData = pQRC->data;
        memset(*abgr, 0xff, unDataBytes);
        for (y = 0; y < unWidth; y++) {
            pDestData = *abgr + unWidthAdjusted * y * zoom_in;
            for (x = 0; x < unWidth; x++) {
                if (*pSourceData & 1) {
                    for (l = 0; l < zoom_in; l++) {
                        for (n = 0; n < zoom_in; n++) {
                            memcpy(
                                pDestData + n * PIXELSIZE + unWidthAdjusted * l,
                                POINTDATA, PIXELSIZE);
                        }
                    }
                }
                pDestData += PIXELSIZE * zoom_in;
                pSourceData++;
            }
        }

        QRcode_free(pQRC);
    } else {
        printf("NULL returned");
        return -1;
    }

    return unDataBytes;
}

int GetQrImg(const char* text, uint8_t** abgr, int size, int border,
             uint32_t fg_color, uint32_t bg_color) {
    if (text == NULL || abgr == NULL || size < 0 || border < 0) {
        return -1;
    }
    int win = size + border * 2;
    uint8_t* qrsrc;
    unsigned int unWidth, x, y, l, n, unWidthAdjusted, unDataBytes;
    unsigned char *pSourceData, *pDestData;
    QRcode* pQRC;
    int qrorgw = size;
    int zoom_in = 1;
    // QR_ECLEVEL_M
    // QR_ECLEVEL_H
    if (pQRC = QRcode_encodeString(text, 0, QR_ECLEVEL_H, QR_MODE_8, 1)) {
        unWidth = pQRC->width;
        zoom_in = size / pQRC->width;
        if (zoom_in <= 0) {
            printf("GetQrImg Dest W too small:%d %d\n", size, pQRC->width);
            QRcode_free(pQRC);
            return -1;
        }
        qrorgw = pQRC->width * zoom_in;
        unWidthAdjusted = unWidth * zoom_in * PIXELSIZE;
        unDataBytes = unWidthAdjusted * unWidth * zoom_in;
        // printf("GetQrImg zoom_in:%d\n", zoom_in);
        qrsrc = (uint8_t*)malloc(unDataBytes);
        if (qrsrc == NULL) {
            printf("GetQrImg Out of memory\n");
            QRcode_free(pQRC);
            return -1;
        }
        // Convert QrCode bits to bmp pixels
        pSourceData = pQRC->data;
        for (int i = 0; i < unDataBytes; i = i + PIXELSIZE) {
            memcpy(&qrsrc[i], (void*)&bg_color, PIXELSIZE);
        }
        for (y = 0; y < unWidth; y++) {
            pDestData = qrsrc + unWidthAdjusted * y * zoom_in;
            for (x = 0; x < unWidth; x++) {
                if (*pSourceData & 1) {
                    for (l = 0; l < zoom_in; l++) {
                        for (n = 0; n < zoom_in; n++) {
                            memcpy(
                                pDestData + n * PIXELSIZE + unWidthAdjusted * l,
                                (void*)&fg_color, PIXELSIZE);
                        }
                    }
                }
                pDestData += PIXELSIZE * zoom_in;
                pSourceData++;
            }
        }

        QRcode_free(pQRC);
    } else {
        printf("GetQrImg NULL returned\n");
        return -1;
    }

    //生成的二维码贴图到居中显示
    // int wout = (int)*w;
    unDataBytes = win * win * PIXELSIZE;
    uint8_t* qrimg = (uint8_t*)malloc(unDataBytes);
    for (int i = 0; i < unDataBytes; i = i + PIXELSIZE) {
        memcpy(&qrimg[i], (void*)&bg_color, PIXELSIZE);
    }
    int offsetx = (win - qrorgw) / 2;
    int offsety = offsetx;
    uint8_t* pdst = NULL;
    uint8_t* psrc = NULL;
    for (int i = 0; i < qrorgw; i++) {
        psrc = qrsrc + i * qrorgw * PIXELSIZE;
        pdst = qrimg + (i + offsety) * win * PIXELSIZE + offsetx * PIXELSIZE;
        memcpy(pdst, psrc, qrorgw * PIXELSIZE);
    }
    *abgr = qrimg;
    free(qrsrc);
    return unDataBytes;
}
