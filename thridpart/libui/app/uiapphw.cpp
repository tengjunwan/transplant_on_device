#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "display/fbdev.h"
#include "gfbg.h"
#include "lvgl.h"
#include "sample_comm.h"
#include "threads.h"
#include "uiapp.h"

#define DISP_BUF_SIZE (LV_HOR_RES_MAX * LV_VER_RES_MAX)
typedef struct {
  int fd;
  unsigned char *fb_base;
  struct fb_var_screeninfo fb_var;
  pthread_t pid;
  int run;
  lv_color_t *disp_buf1;
} stUiParams;

typedef struct {
  int width;
  int height;
  int screen_size;
  int line_width;
  int bpp;
  int pixel_width;
} screen_struct;

static stUiParams stUiParam;      /* framebuffer */
static screen_struct screen_info; /* scree */
// static stUiPageParams stPages;
#define DEFAULT_LINUX_FB_PATH "/dev/fb0"
static lv_disp_drv_t disp_drv;

static struct fb_bitfield g_a32 = {24, 8, 0};
static struct fb_bitfield g_r32 = {16, 8, 0};
static struct fb_bitfield g_g32 = {8, 8, 0};
static struct fb_bitfield g_b32 = {0, 8, 0};
struct fb_fix_screeninfo fix;
static pthread_mutex_t Uilock = PTHREAD_MUTEX_INITIALIZER;
void fbDispFlush(lv_disp_drv_t *disp, const lv_area_t *area,
                 lv_color_t *color_p);
static td_void *uiprocess(td_void *args);

uint16_t fb_xres = 1920;
uint16_t fb_yres = 1080;
uint16_t GetUi_xres() { return fb_xres; }
uint16_t GetUi_yres() { return fb_yres; }

td_s32 my_fb_init() {
  stUiParam.fd = open(DEFAULT_LINUX_FB_PATH, O_RDWR, 0);
  if (stUiParam.fd < 0) {
    printf("open %s failed!\n", DEFAULT_LINUX_FB_PATH);
    return TD_FAILURE;
  }

  ot_fb_point point = {0, 0};
  point.x_pos = 0;
  point.y_pos = 0;

  if (ioctl(stUiParam.fd, FBIOPUT_SCREEN_ORIGIN_GFBG, &point) < 0) {
    sample_print("set screen original show position failed!\n");
    close(stUiParam.fd);
    stUiParam.fd = -1;
    return TD_FAILURE;
  }

  if (ioctl(stUiParam.fd, FBIOGET_VSCREENINFO, &stUiParam.fb_var) < 0) {
    sample_print("get variable screen info failed!\n");
    return TD_FAILURE;
  }

  stUiParam.fb_var.transp = g_a32;
  stUiParam.fb_var.red = g_r32;
  stUiParam.fb_var.green = g_g32;
  stUiParam.fb_var.blue = g_b32;
  stUiParam.fb_var.bits_per_pixel = 32; /* 32 for 4 byte */
  stUiParam.fb_var.xres_virtual = GetUi_xres();
  stUiParam.fb_var.yres_virtual = GetUi_yres();
  stUiParam.fb_var.xres = GetUi_xres();
  stUiParam.fb_var.yres = GetUi_yres();
  stUiParam.fb_var.activate = FB_ACTIVATE_NOW;

  if (ioctl(stUiParam.fd, FBIOPUT_VSCREENINFO, &stUiParam.fb_var) < 0) {
    sample_print("put variable screen info failed!\n");
    return TD_FAILURE;
  }

  if (ioctl(stUiParam.fd, FBIOGET_VSCREENINFO, &stUiParam.fb_var) < 0) {
    sample_print("get variable screen info failed!\n");
  }

  if (ioctl(stUiParam.fd, FBIOGET_FSCREENINFO, &fix) < 0) {
    sample_print("get fix screen info failed!\n");
  }
  td_u32 fix_screen_stride;
  fix_screen_stride = fix.line_length;
  stUiParam.fb_base =
      (unsigned char *)mmap(TD_NULL, fix.smem_len, PROT_READ | PROT_WRITE,
                            MAP_SHARED, stUiParam.fd, 0);
  if (stUiParam.fb_base == MAP_FAILED) {
    sample_print("mmap framebuffer failed!\n");
  }
  memset_s(stUiParam.fb_base, fix.smem_len, 0x00, fix.smem_len);
  td_bool show;
  show = TD_TRUE;
  if (ioctl(stUiParam.fd, FBIOPUT_SHOW_GFBG, &show) < 0) {
    sample_print("FBIOPUT_SHOW_GFBG failed!\n");
    return TD_FAILURE;
  }

  screen_info.width = stUiParam.fb_var.xres;
  screen_info.height = stUiParam.fb_var.yres;
  screen_info.bpp = stUiParam.fb_var.bits_per_pixel;
  screen_info.line_width =
      stUiParam.fb_var.xres * stUiParam.fb_var.bits_per_pixel / 8;
  screen_info.pixel_width = stUiParam.fb_var.bits_per_pixel / 8;
  screen_info.screen_size = stUiParam.fb_var.xres * stUiParam.fb_var.yres *
                            stUiParam.fb_var.bits_per_pixel / 8;

  return 0;
}
void fbDispFlush(lv_disp_drv_t *disp, const lv_area_t *area,
                 lv_color_t *color_p) {
  int32_t x, y;
  if (area->y1 > screen_info.height || area->y2 > screen_info.height ||
      area->x1 > screen_info.width || area->x2 > screen_info.width) {
    return;
  }
  uint32_t *argb8888;

  for (y = area->y1; y <= area->y2; y++) {
    for (x = area->x1; x <= area->x2; x++) {
      uint32_t *offset =
          (uint32_t *)(stUiParam.fb_base + x * screen_info.pixel_width +
                       y * screen_info.line_width);

      argb8888 = (uint32_t *)offset;
      argb8888 = (uint32_t *)offset;
      *argb8888 = ((color_p->ch.alpha) << 24) | ((color_p->ch.blue) << 16) |
                  ((color_p->ch.green) << 8) | (color_p->ch.red);
      color_p++;
    }
  }

  lv_disp_flush_ready(disp);
}
uint32_t custom_tick_get(void) {
  static uint64_t start_ms = 0;
  if (start_ms == 0) {
    struct timeval tv_start;
    gettimeofday(&tv_start, NULL);
    start_ms = (tv_start.tv_sec * 1000000 + tv_start.tv_usec) / 1000;
  }
  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  uint64_t now_ms;
  now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;

  uint32_t time_ms = now_ms - start_ms;
  return time_ms;
}

static int uiinit = 0;
int UiAppMain() {
  stUiParam.run = 1;
  lv_init();
  my_fb_init();
  static lv_disp_draw_buf_t disp_buf1;
  static lv_color_t buf1_1[1920 * 120];
  lv_disp_draw_buf_init(&disp_buf1, buf1_1, NULL, LV_HOR_RES_MAX * 120);
  lv_disp_drv_init(&disp_drv); /*Basic initialization*/
  disp_drv.draw_buf = &disp_buf1;
  disp_drv.hor_res = LV_HOR_RES_MAX;
  disp_drv.ver_res = LV_VER_RES_MAX;
  disp_drv.flush_cb = fbDispFlush;
  lv_disp_drv_register(&disp_drv);
  CreateUi();
  pthread_create(&stUiParam.pid, 0, uiprocess, NULL);
  uiinit = 1;
  return 0;
}

int UiAppMainStop() {
  if (uiinit == 0) {
    return 0;
  }
  UiExit();
  stUiParam.run = 0;
  pthread_join(stUiParam.pid, 0);
  munmap(stUiParam.fb_base, fix.smem_len);
  td_bool show = TD_FALSE;
  if (ioctl(stUiParam.fd, FBIOPUT_SHOW_GFBG, &show) < 0) {
    close(stUiParam.fd);
    return TD_FAILURE;
  }
  close(stUiParam.fd);
  stUiParam.fd = -1;
  free(stUiParam.disp_buf1);
  return 0;
}

static td_void *uiprocess(td_void *args) {
  prctl(PR_SET_NAME, "uiprocess", 0, 0, 0);
  while (stUiParam.run) {
    pthread_mutex_lock(&Uilock);
    lv_task_handler();
    pthread_mutex_unlock(&Uilock);
    ot_usleep(50 * 1000);
  }
}

#define MAXOBJNUM 50
typedef struct {
  lv_obj_t *label[MAXOBJNUM];
} stUiPageParams;

stUiPageParams stPages;
LV_FONT_DECLARE(lv_font_simsun_16)

void UiExit() {}
void CreateUi() {
  static lv_style_t style_scr_act;
  if (style_scr_act.prop_cnt == 0) {
    lv_style_init(&style_scr_act);
    lv_style_set_bg_opa(&style_scr_act, LV_OPA_COVER);
    lv_obj_add_style(lv_scr_act(), &style_scr_act, 0);
  }
  lv_disp_get_default()->driver->screen_transp = 1;
  lv_disp_set_bg_opa(lv_disp_get_default(), LV_OPA_TRANSP);
  lv_memset_00(lv_disp_get_default()->driver->draw_buf->buf_act,
               lv_disp_get_default()->driver->draw_buf->size *
                   sizeof(lv_color32_t));
  lv_style_set_bg_opa(&style_scr_act, LV_OPA_TRANSP);
  lv_obj_report_style_change(&style_scr_act);

  // for (size_t i = 0; i < MAXOBJNUM; i++) {
  //   stPages.label[i] = lv_label_create(lv_scr_act());
  //   lv_label_set_text(stPages.label[i], "xxxx");
  //   lv_obj_align(stPages.label[i], LV_ALIGN_TOP_LEFT, 10, 30);
  //   lv_obj_set_style_text_font(stPages.label[i], &lv_font_simsun_16, 0);
  //   lv_obj_set_style_text_color(stPages.label[i],
  //                               lv_color_make(0xFF, 0x00, 0x00), 0);
  // }
}
int labcount = 0;
void DrawOsdInfo(stYolovDetectObjs *pOut) {
  pthread_mutex_lock(&Uilock);
  for (size_t i = 0; i < labcount; i++) {
    lv_obj_del(stPages.label[i]);
  }

  labcount = 0;
  for (size_t i = 0; i < pOut->id_count; i++) {

    stPages.label[i] = lv_label_create(lv_scr_act());
    lv_label_set_text_fmt(stPages.label[i], "ID:%d", pOut->id_objs[i].id);
    lv_obj_align(stPages.label[i], LV_ALIGN_TOP_LEFT, pOut->id_objs[i].x,
                 pOut->id_objs[i].y);
    lv_obj_set_style_text_font(stPages.label[i], &lv_font_simsun_16, 0);
    lv_obj_set_style_text_color(stPages.label[i],
                                lv_color_make(0x00, 0xFF, 0x00), 0);
  }
  labcount = pOut->id_count;
  pthread_mutex_unlock(&Uilock);
}