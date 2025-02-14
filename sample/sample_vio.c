/*
  Copyright (c), 2001-2022, Shenshu Tech. Co., Ltd.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "sample_comm.h"
#include "securec.h"
#include "sample_common_ive.h"
#include "uiapp.h"
#include "sample_npu_process.h"
#include "detectobjs.h"
#include "uiapp.h"
#include "wrapperDeepSort.h"

static volatile sig_atomic_t g_sig_flag = 0;

#define X_ALIGN 16
#define Y_ALIGN 2
#define out_ratio_1(x) ((x) / 3)
#define out_ratio_2(x) ((x) * 2 / 3)
#define out_ratio_3(x) ((x) / 2)
#define check_digit(x) ((x) >= '0' && (x) <= '9')

#define VB_RAW_CNT_NONE     0
#define VB_LINEAR_RAW_CNT   5
#define VB_WDR_RAW_CNT      8
#define VB_MULTI_RAW_CNT    15
#define VB_YUV_ROUTE_CNT    10
#define VB_DOUBLE_YUV_CNT   15
#define VB_MULTI_YUV_CNT    30

// ot_vb_blk vb_blk;
pthread_t nnn_pid;
pthread_t vodrawrc_pid;
static int nnn_thd_run = 0;
stYolovDetectObjs* pDrawInfo;
static pthread_mutex_t algolock = PTHREAD_MUTEX_INITIALIZER;

#define FEATURE_SORT 1

static sample_vo_cfg g_vo_cfg = {
    .vo_dev = SAMPLE_VO_DEV_UHD,
    .vo_intf_type = OT_VO_INTF_HDMI,
    .intf_sync = OT_VO_OUT_1080P60,
    .bg_color = COLOR_RGB_BLACK,
    .pix_format = OT_PIXEL_FORMAT_YVU_SEMIPLANAR_420,
    .disp_rect = {0, 0, 1920, 1080},
    .image_size = {1920, 1080},
    .vo_part_mode = OT_VO_PARTITION_MODE_SINGLE,
    .dis_buf_len = 3, /* 3: def buf len for single */
    .dst_dynamic_range = OT_DYNAMIC_RANGE_SDR8,
    .vo_mode = VO_MODE_2MUX,
    .compress_mode = OT_COMPRESS_MODE_NONE,
};

static td_void sample_get_char(td_void) {
    if (g_sig_flag == 1) {
        return;
    }
    sample_pause();
}

static td_void save_yuv420sp(const char *filename, ot_svp_img *img) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        printf("failed to open file %s\n", filename);
        return;
    }

    // write Y plane (640 * 640)
    fwrite((void*)img->virt_addr[0], 1, img->width * img->height, fp);

    // write UV plane (320 * 320)
    fwrite((void*)img->virt_addr[1], 1, (img->width * img->height) / 2, fp);

    fclose(fp);
    printf("save YUV image to %s\n", filename);
}


static td_void save_rgb(const char *filename, ot_svp_img *img) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        printf("failed to open file %s\n", filename);
        return;
    }


    td_s32 size = img->width * img->height * 3;
    fwrite((void*)img->virt_addr[0], 1, size, fp);

    fclose(fp);
    printf("save RGB image to %s\n", filename);
}

static td_void sample_vi_get_default_vb_config(ot_size* size, ot_vb_cfg* vb_cfg, ot_vi_video_mode video_mode,
        td_u32 yuv_cnt, td_u32 raw_cnt) {
    ot_vb_calc_cfg calc_cfg;
    ot_pic_buf_attr buf_attr;

    (td_void)memset_s(vb_cfg, sizeof(ot_vb_cfg), 0, sizeof(ot_vb_cfg));
    vb_cfg->max_pool_cnt = 128; /* 128 blks */

    /* default YUV pool: SP420 + compress_seg */
    buf_attr.width = size->width;
    buf_attr.height = size->height;
    buf_attr.align = OT_DEFAULT_ALIGN;
    buf_attr.bit_width = OT_DATA_BIT_WIDTH_8;
    buf_attr.pixel_format = OT_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    buf_attr.compress_mode = OT_COMPRESS_MODE_SEG;
    ot_common_get_pic_buf_cfg(&buf_attr, &calc_cfg);

    vb_cfg->common_pool[0].blk_size = calc_cfg.vb_size;
    vb_cfg->common_pool[0].blk_cnt = yuv_cnt;

    /* default raw pool: raw12bpp + compress_line */
    buf_attr.pixel_format = OT_PIXEL_FORMAT_RGB_BAYER_12BPP;
    buf_attr.compress_mode = (video_mode == OT_VI_VIDEO_MODE_NORM ? OT_COMPRESS_MODE_LINE : OT_COMPRESS_MODE_NONE);
    ot_common_get_pic_buf_cfg(&buf_attr, &calc_cfg);
    vb_cfg->common_pool[1].blk_size = calc_cfg.vb_size;
    vb_cfg->common_pool[1].blk_cnt = raw_cnt;
}

static td_s32 sample_vio_sys_init(ot_vi_vpss_mode_type mode_type, ot_vi_video_mode video_mode,
        td_u32 yuv_cnt, td_u32 raw_cnt) {
    td_s32 ret;
    ot_size size;
    ot_vb_cfg vb_cfg;
    td_u32 supplement_config;
    sample_sns_type sns_type = SENSOR0_TYPE;

    sample_comm_vi_get_size_by_sns_type(sns_type, &size);
    sample_vi_get_default_vb_config(&size, &vb_cfg, video_mode, yuv_cnt, raw_cnt);

    supplement_config = OT_VB_SUPPLEMENT_BNR_MOT_MASK;
    ret = sample_comm_sys_init_with_vb_supplement(&vb_cfg, supplement_config);
    if (ret != TD_SUCCESS) {
        return TD_FAILURE;
    }

    ret = sample_comm_vi_set_vi_vpss_mode(mode_type, video_mode);
    if (ret != TD_SUCCESS) {
        return TD_FAILURE;
    }

    return TD_SUCCESS;
}

static td_s32 sample_vio_start_vpss(ot_vpss_grp grp, ot_size* in_size) {
    td_s32 ret;
    ot_vpss_grp_attr grp_attr;
    ot_vpss_chn_attr chn_attr;
    td_bool chn_enable[OT_VPSS_MAX_PHYS_CHN_NUM] = { TD_TRUE, TD_TRUE, TD_FALSE, TD_FALSE };

    sample_comm_vpss_get_default_grp_attr(&grp_attr);
    grp_attr.max_width = in_size->width;
    grp_attr.max_height = in_size->height;
    sample_comm_vpss_get_default_chn_attr(&chn_attr);
    chn_attr.width = in_size->width;
    chn_attr.height = in_size->height;

    ot_vpss_chn_attr chn_attrex[2];
    memcpy(&chn_attrex[0], &chn_attr, sizeof(chn_attr));
    memcpy(&chn_attrex[1], &chn_attr, sizeof(chn_attr));
    // chn_attrex[1].width = 640;
    // chn_attrex[1].height = 640;
    chn_attrex[1].width = 1280;
    chn_attrex[1].height = 1280;
    chn_attrex[1].compress_mode = OT_COMPRESS_MODE_NONE;
    chn_attrex[1].depth = 1;

    chn_attrex[0].depth = 1;
    ret = sample_common_vpss_start(grp, chn_enable, &grp_attr, chn_attrex, OT_VPSS_MAX_PHYS_CHN_NUM);
    if (ret != TD_SUCCESS) {
        return ret;
    }
    return TD_SUCCESS;
}

static td_void sample_vio_stop_vpss(ot_vpss_grp grp) {
    td_bool chn_enable[OT_VPSS_MAX_PHYS_CHN_NUM] = { TD_TRUE, TD_FALSE, TD_FALSE, TD_FALSE };

    sample_common_vpss_stop(grp, chn_enable, OT_VPSS_MAX_PHYS_CHN_NUM);
}

static td_s32 sample_vio_start_vo(sample_vo_mode vo_mode) {
    g_vo_cfg.vo_mode = vo_mode;

    return sample_comm_vo_start_vo(&g_vo_cfg);
}

static td_void sample_vio_stop_vo(td_void) {
    sample_comm_vo_stop_vo(&g_vo_cfg);
}

static td_s32 sample_vio_start_venc_and_vo(ot_vpss_grp vpss_grp[], td_u32 grp_num, const ot_size* in_size) {
    td_u32 i;
    td_s32 ret;
    sample_vo_mode vo_mode = VO_MODE_1MUX;
    const ot_vpss_chn vpss_chn = 0;
    const ot_vo_layer vo_layer = 0;
    ot_vo_chn vo_chn[4] = { 0, 1, 2, 3 };     /* 4: max chn num, 0/1/2/3 chn id */

    if (grp_num > 1) {
        vo_mode = VO_MODE_4MUX;
    }

    ret = sample_vio_start_vo(vo_mode);
    if (ret != TD_SUCCESS) {
        goto start_vo_failed;
    }

    for (i = 0; i < grp_num; i++) {
        sample_comm_vpss_bind_vo(vpss_grp[i], vpss_chn, vo_layer, vo_chn[i]);
    }
    return TD_SUCCESS;

start_vo_failed:
    return TD_FAILURE;
}

static td_void sample_vio_stop_venc_and_vo(ot_vpss_grp vpss_grp[], td_u32 grp_num) {
    td_u32 i;
    const ot_vpss_chn vpss_chn = 0;
    const ot_vo_layer vo_layer = 0;
    ot_vo_chn vo_chn[4] = { 0, 1, 2, 3 };     /* 4: max chn num, 0/1/2/3 chn id */

    for (i = 0; i < grp_num; i++) {
        sample_comm_vpss_un_bind_vo(vpss_grp[i], vpss_chn, vo_layer, vo_chn[i]);
    }
    sample_vio_stop_vo();
}


long getms() {
    struct timeval start;
    gettimeofday(&start, NULL);
    long ms = (start.tv_sec) * 1000 + (start.tv_usec) / 1000;
    return ms;
}

int vgsdraw(ot_video_frame_info* pframe, stYolovDetectObjs* pOut) {
    td_s32 ret;
    if (!pOut && pOut->id_count <= 0) {
        return -1;
    }
    ot_vgs_handle h_handle = -1;
    ot_vgs_task_attr vgs_task_attr = { 0 };
    static ot_vgs_line stLines[OBJDETECTMAX];
    int thick = 2;
    int color = 0x00FF00;
    int colors[7] = { 0x0000ff,0x00FF00,0xff0000,0x00FFff,0xffFF00,0x000000,0xffffff };

    int linecount = 0;
    for (int i = 0; i < pOut->id_count; i++) {
        int xs = pOut->id_objs[i].x & 0xFFFE;
        int ys = pOut->id_objs[i].y & 0xFFFE;
        int xe = (pOut->id_objs[i].x + pOut->id_objs[i].w) & 0xFFFE;
        int ye = (pOut->id_objs[i].y + pOut->id_objs[i].h) & 0xFFFE;
        if (pOut->id_objs[i].w < 0 || pOut->id_objs[i].w > pframe->video_frame.width) {
            continue;
        }
        if (pOut->id_objs[i].h < 0 || pOut->id_objs[i].h >  pframe->video_frame.height) {
            continue;
        }
        color = colors[pOut->id_objs[i].id % 7];
        // 上方的线条
        stLines[linecount].color = color;
        stLines[linecount].thick = thick;
        stLines[linecount].start_point.x = xs;
        stLines[linecount].start_point.y = ys;
        stLines[linecount].end_point.x = xe;
        stLines[linecount].end_point.y = ys;
        linecount++;

        // 左边的线条
        stLines[linecount].color = color;
        stLines[linecount].thick = thick;
        stLines[linecount].start_point.x = xs;
        stLines[linecount].start_point.y = ys;
        stLines[linecount].end_point.x = xs;
        stLines[linecount].end_point.y = ye;
        linecount++;

        // 右边线条
        stLines[linecount].color = color;
        stLines[linecount].thick = thick;
        stLines[linecount].start_point.x = xe;
        stLines[linecount].start_point.y = ys;
        stLines[linecount].end_point.x = xe;
        stLines[linecount].end_point.y = ye;
        linecount++;

        // 下边线条
        stLines[linecount].color = color;
        stLines[linecount].thick = thick;
        stLines[linecount].start_point.x = xs;
        stLines[linecount].start_point.y = ye;
        stLines[linecount].end_point.x = xe;
        stLines[linecount].end_point.y = ye;
        linecount++;
    }

    if (linecount <= 0) {
        return -1;
    }
    // printf("linecount:%d\n",linecount);
    ret = ss_mpi_vgs_begin_job(&h_handle);
    if (ret != TD_SUCCESS) {
        return TD_FAILURE;
    }
    if (memcpy_s(&vgs_task_attr.img_in, sizeof(ot_video_frame_info), pframe,
        sizeof(ot_video_frame_info)) != EOK) {
        return TD_FAILURE;
    }
    if (memcpy_s(&vgs_task_attr.img_out, sizeof(ot_video_frame_info), pframe,
        sizeof(ot_video_frame_info)) != EOK) {
        return TD_FAILURE;
    }

    ret = ss_mpi_vgs_add_draw_line_task(h_handle, &vgs_task_attr, stLines,
        linecount);
    if (ret != TD_SUCCESS) {
        ss_mpi_vgs_cancel_job(h_handle);
        printf("ss_mpi_vgs_add_draw_line_task ret:%08X\n", ret);
        return TD_FAILURE;
    }

    /* step5: start VGS work */
    ret = ss_mpi_vgs_end_job(h_handle);
    if (ret != TD_SUCCESS) {
        ss_mpi_vgs_cancel_job(h_handle);
        return TD_FAILURE;
    }

    return ret;
}

static td_s32 framecpy(ot_svp_dst_img* dstf,
    ot_video_frame_info* srcf) {
    td_s32 ret = OT_ERR_IVE_NULL_PTR;
    ot_ive_handle handle;
    ot_svp_src_data src_data;
    ot_svp_dst_data dst_data;
    ot_ive_dma_ctrl ctrl = { OT_IVE_DMA_MODE_DIRECT_COPY, 0, 0, 0, 0 };
    td_bool is_finish = TD_FALSE;
    td_bool is_block = TD_TRUE;
    // Copy Y
    int w = dstf->width;
    int h = dstf->height;
    src_data.phys_addr = srcf->video_frame.phys_addr[0];
    src_data.width = w;   // w;
    src_data.height = h;  // h;
    src_data.stride = dstf->stride[0];

    dst_data.phys_addr = dstf->phys_addr[0];
    dst_data.width = w;   // w;
    dst_data.height = h;  // h;
    dst_data.stride = dstf->stride[0];
    ret = ss_mpi_ive_dma(&handle, &src_data, &dst_data, &ctrl, TD_TRUE);
    if (ret != TD_SUCCESS) {

        return -1;
    }
    ret = ss_mpi_ive_query(handle, &is_finish, is_block);
    while (ret == OT_ERR_IVE_QUERY_TIMEOUT) {
        usleep(100);
        ret = ss_mpi_ive_query(handle, &is_finish, is_block);
    }
    // Copy UV
    src_data.phys_addr = srcf->video_frame.phys_addr[1];
    src_data.width = w;   // w;
    src_data.height = h / 2;  // h;
    src_data.stride = dstf->stride[0];

    dst_data.phys_addr = dstf->phys_addr[1];
    dst_data.width = w;   // w;
    dst_data.height = h / 2;  // h;
    dst_data.stride = dstf->stride[0];;
    ret = ss_mpi_ive_dma(&handle, &src_data, &dst_data, &ctrl, TD_TRUE);
    if (ret != TD_SUCCESS) {

        return -1;
    }
    ret = ss_mpi_ive_query(handle, &is_finish, is_block);
    while (ret == OT_ERR_IVE_QUERY_TIMEOUT) {
        usleep(100);
        ret = ss_mpi_ive_query(handle, &is_finish, is_block);
    }
    return TD_SUCCESS;
}


static td_s32 frame_crop(ot_svp_dst_img* dstf, ot_video_frame_info* srcf, 
    td_s32 x_crop, td_s32 y_crop) {
    
    td_s32 ret = OT_ERR_IVE_NULL_PTR;
    ot_ive_handle handle;
    ot_svp_dst_data dst_data;
    ot_svp_src_data src_data;
    td_bool is_finish = TD_FALSE;
    td_bool is_block = TD_TRUE;
    // ot_ive_dma_ctrl ctrl = { OT_IVE_DMA_MODE_INTERVAL_COPY, 0, 0, 0, 0}; // enables resizing
    ot_ive_dma_ctrl ctrl = { OT_IVE_DMA_MODE_DIRECT_COPY, 0, 0, 0, 0}; 

    // adjust crop parameters to be even
    x_crop &= ~1;
    y_crop &= ~1;
    td_s32 w_crop = dstf->width;
    td_s32 h_crop = dstf->height;
    // printf("crop params: x=%d, y=%d, w=%d, h=%d\n", x_crop, y_crop, w_crop, h_crop);
    // printf("srcf->video_frame.stride[0] = %d\n", srcf->video_frame.stride[0]);
    // printf("srcf->video_frame.stride[1] = %d\n", srcf->video_frame.stride[1]);
    // printf("srcf->video_frame.width = %d\n", srcf->video_frame.width);
    // printf("srcf->video_frame.height = %d\n", srcf->video_frame.height);
    // printf("dstf->stride[0] = %d\n", dstf->stride[0]);
    // printf("dstf->stride[1] = %d\n", dstf->stride[1]);
    // printf("dstf->width = %d\n", dstf->width);
    // printf("dstf->height = %d\n", dstf->height);

    // copy & resize Y plane
    src_data.phys_addr = srcf->video_frame.phys_addr[0] + y_crop * srcf->video_frame.stride[0] + x_crop;
    src_data.width = w_crop;
    src_data.height = h_crop;
    src_data.stride = srcf->video_frame.stride[0];

    dst_data.phys_addr = dstf->phys_addr[0];
    dst_data.width = dstf->width;
    dst_data.height = dstf->height;
    dst_data.stride = dstf->stride[0];
    

    ret = ss_mpi_ive_dma(&handle, &src_data, &dst_data, &ctrl, TD_TRUE);
    if (ret != TD_SUCCESS) return -1;

    ret = ss_mpi_ive_query(handle, &is_finish, is_block);
    while (ret == OT_ERR_IVE_QUERY_TIMEOUT) {
        usleep(100);
        ret = ss_mpi_ive_query(handle, &is_finish, is_block);
    }

    // copy & resize UV plane
    src_data.phys_addr = srcf->video_frame.phys_addr[1] + (y_crop / 2) * srcf->video_frame.stride[0] + x_crop;
    src_data.width = w_crop;
    src_data.height = h_crop / 2;
    src_data.stride = srcf->video_frame.stride[0];

    dst_data.phys_addr = dstf->phys_addr[1];
    dst_data.width = dstf->width;
    dst_data.height = dstf->height / 2;
    dst_data.stride = dstf->stride[0];

    ret = ss_mpi_ive_dma(&handle, &src_data, &dst_data, &ctrl, TD_TRUE);
    if (ret != TD_SUCCESS) return -1;

    ret = ss_mpi_ive_query(handle, &is_finish, is_block);
    while (ret == OT_ERR_IVE_QUERY_TIMEOUT) {
        usleep(100);
        ret = ss_mpi_ive_query(handle, &is_finish, is_block);
    }

    return TD_SUCCESS;
}


td_s32 frame_YUV420SP2RGB(ot_svp_img* srcYUV, ot_svp_img* dstRGB) {
    td_s32 ret;
    ot_ive_handle handle;
    ot_ive_csc_ctrl csc_ctrl;

    // set color conversion mode: YUV420SP -> RGB
    csc_ctrl.mode = OT_IVE_CSC_MODE_PIC_BT709_YUV_TO_RGB;

    ret = ss_mpi_ive_csc(&handle, (const ot_svp_src_img*)srcYUV, (const ot_svp_dst_img*)dstRGB, 
                          (const ot_ive_csc_ctrl*)&csc_ctrl, TD_TRUE);
    if (ret != TD_SUCCESS) {
        printf("Error: YUV420SP to RGB conversion failed!\n");
        return ret;
    }

    // wait for the operation to complete
    td_bool is_finish = TD_FALSE;
    td_bool is_block = TD_TRUE;
    ret = ss_mpi_ive_query(handle, &is_finish, is_block);
    while (ret == OT_ERR_IVE_QUERY_TIMEOUT) {
        usleep(100);
        ret  = ss_mpi_ive_query(handle, &is_finish, is_block);
    }

    return TD_SUCCESS;
}

td_s32 frame_resize(ot_svp_img* srcRGB, ot_svp_img* dstRGB) {
    td_s32 ret;
    ot_ive_handle handle;
    ot_ive_resize_ctrl resize_ctrl;

    printf("srcRGB: w=%d, h=%d, stride[0]=%d, stride[1]=%d, stride[2]=%d\n",
           srcRGB->width, srcRGB->height, srcRGB->stride[0], srcRGB->stride[1], srcRGB->stride[2]);
    printf("dstRGB: w=%d, h=%d, stride[0]=%d, stride[1]=%d, stride[2]=%d\n",
           dstRGB->width, dstRGB->height, dstRGB->stride[0], dstRGB->stride[1], dstRGB->stride[2]);
    printf("srcRGB type: %d, dstRGB type: %d\n", srcRGB->type, dstRGB->type);


    // image arrays
    ot_svp_img src_array[1] = {*srcRGB};
    ot_svp_img dst_array[1] = {*dstRGB};

    // set up resize control parameters
    resize_ctrl.mode = OT_IVE_RESIZE_MODE_LINEAR; // bilinear interpolation
    resize_ctrl.num = 1; // processing a single image

    // allocate exxtra memory required
    td_u32 U8C1_NUM = 0; // since we're dealing with U8C3_PLANAR
    td_u32 mem_size = 25 * U8C1_NUM + 49 * (resize_ctrl.num - U8C1_NUM);
    printf("required mem size: %lu\n", mem_size);
    // td_u32 mem_size = 10000;
    memset(&resize_ctrl.mem, 0, sizeof(resize_ctrl.mem)); //  zero out
    printf("(before)resize_ctrl.mem allocation, phys_addr: %lx, virt_addr: %p, size: %lu\n", 
           resize_ctrl.mem.phys_addr, (void*)resize_ctrl.mem.virt_addr, resize_ctrl.mem.size);
    td_s32 ret_mem = ss_mpi_sys_mmz_alloc(&resize_ctrl.mem.phys_addr, (td_void**)&resize_ctrl.mem.virt_addr,
                                          "resize_mem", TD_NULL, mem_size);
    resize_ctrl.mem.size = mem_size;
    if (ret_mem != TD_SUCCESS) {
        printf("Errror: failed to allocate memory for resize_ctrl.mem!\n");
        return TD_FAILURE;
    }
    printf("resize_ctrl.mem allocation, phys_addr: %lx, virt_addr: %p, size: %lu\n", 
           resize_ctrl.mem.phys_addr, (void*)resize_ctrl.mem.virt_addr, resize_ctrl.mem.size);


    // performing resizing
    ret = ss_mpi_ive_resize(&handle, src_array, dst_array, &resize_ctrl, TD_TRUE);
    if (ret != TD_SUCCESS) {
        printf("Error: RGB resize failed!\n");
        printf("error code: 0x%08X\n", ret);
        ss_mpi_sys_mmz_free(resize_ctrl.mem.phys_addr, resize_ctrl.mem.virt_addr);
        return ret;
    }

    // Query to check if the resizing is finished
    td_bool is_finish = TD_FALSE;
    td_bool is_block = TD_TRUE;
    ret = ss_mpi_ive_query(handle, &is_finish, is_block);
    while (ret == OT_ERR_IVE_QUERY_TIMEOUT) {
        usleep(100);
        ret = ss_mpi_ive_query(handle, &is_finish, is_block);
    }
    // free the allocated memory
    ss_mpi_sys_mmz_free(resize_ctrl.mem.phys_addr, resize_ctrl.mem.virt_addr);

    return TD_SUCCESS;
}




// td_s32 CreateUsrFrame(ot_svp_img* imgAlgo, int w, int h) {
//     int vbsize = w * h * 3 / 2;
//     vb_blk = ss_mpi_vb_get_blk(OT_VB_INVALID_POOL_ID, vbsize, TD_NULL);
//     imgAlgo->phys_addr[0] = ss_mpi_vb_handle_to_phys_addr(vb_blk);
//     imgAlgo->phys_addr[1] = imgAlgo->phys_addr[0] + w * h;
//     imgAlgo->virt_addr[0] = (td_u64)(td_u8*)ss_mpi_sys_mmap(
//         imgAlgo->phys_addr[0], vbsize);
//     imgAlgo->virt_addr[1] = imgAlgo->virt_addr[0] + w * h;
//     imgAlgo->stride[0] = w;
//     imgAlgo->stride[1] = w;
//     imgAlgo->width = w;
//     imgAlgo->height = h;
//     imgAlgo->type = OT_SVP_IMG_TYPE_YUV420SP;
//     return 0;
// }

ot_vb_blk CreateYUV420SPFrame(ot_svp_img* img, td_s32 w, td_s32 h) {
    w &= ~1; // make w even
    h &= ~1; // make h even

    // calculate required memory size for YUV420SP
    td_s32 vbsize = w * h * 3 / 2;

    // allocate memory using Video Buffer Block(VB)
    ot_vb_blk vb_blk = ss_mpi_vb_get_blk(OT_VB_INVALID_POOL_ID, vbsize, TD_NULL);
    if (vb_blk == OT_VB_INVALID_HANDLE) {
        printf("Error: Failed to allcoate VB block for YUV420SP!\n");
        return OT_VB_INVALID_HANDLE;
    }

    img->phys_addr[0] = ss_mpi_vb_handle_to_phys_addr(vb_blk);
    if (img->phys_addr[0] == 0) {
        printf("Error: Failed to get physical address for YUV420SP!\n");
        ss_mpi_vb_release_blk(vb_blk);
        return OT_VB_INVALID_HANDLE;
    }
    img->phys_addr[1] = img->phys_addr[0] + w * h;
    img->virt_addr[0] = (td_u64)(td_u8*)ss_mpi_sys_mmap(
        img->phys_addr[0], vbsize);
    if (img->virt_addr[0] == 0) {
        printf("Error: Failed to get virtual address for YUV420SP!\n");
        ss_mpi_vb_release_blk(vb_blk);
        return OT_VB_INVALID_HANDLE;
    }
    img->virt_addr[1] = img->virt_addr[0] + w * h;
    img->stride[0] = w;
    img->stride[1] = w;
    img->width = w;
    img->height = h;
    img->type = OT_SVP_IMG_TYPE_YUV420SP;

    printf("YUV frame: w=%d, h=%d, stride[0]=%d, stride[1]=%d\n", img->width, img->height, img->stride[0], img->stride[1]);

    return vb_blk; // return allocated block
}


ot_vb_blk CreateRGBFrame(ot_svp_img* img, td_s32 w, td_s32 h) {
    w &= ~1; // make w even
    h &= ~1; // make h even

    // calculate required memory size for RGB
    td_s32 vbsize = w * h * 3;

    // allocate memory using Video Buffer Block(VB)
    ot_vb_blk vb_blk = ss_mpi_vb_get_blk(OT_VB_INVALID_POOL_ID, vbsize, TD_NULL);
    if (vb_blk == OT_VB_INVALID_HANDLE) {
        printf("Error: Failed to allcoate VB block! for RGB frame\n");
        return OT_VB_INVALID_HANDLE;
    }

    img->phys_addr[0] = ss_mpi_vb_handle_to_phys_addr(vb_blk); // B plane
    img->phys_addr[1] = img->phys_addr[0] + w * h; // G plane
    img->phys_addr[2] = img->phys_addr[1] + w * h; // R plane
    if (img->phys_addr[0] == 0) {
        printf("Error: Failed to get physical address! for RGB frame\n");
        ss_mpi_vb_release_blk(vb_blk);
        return OT_VB_INVALID_HANDLE;
    }

    
    img->virt_addr[0] = (td_u64)(td_u8*)ss_mpi_sys_mmap(img->phys_addr[0], vbsize);
    img->virt_addr[1] = img->virt_addr[0] + w * h; // G plane
    img->virt_addr[2] = img->virt_addr[1] + w * h; // R plane
    if (img->virt_addr[0] == 0) {
        printf("Error: Failed to get virtual address! for RGB frame\n");
        ss_mpi_vb_release_blk(vb_blk);
        return OT_VB_INVALID_HANDLE;
    }

    img->stride[0] = w;
    img->stride[1] = w;
    img->stride[2] = w;
    img->width = w;
    img->height = h;
    img->type = OT_SVP_IMG_TYPE_U8C3_PLANAR;

    printf("RGB frame: w=%d, h=%d, stride[0]=%d, stride[1]=%d, stride[2]=%d\n", 
            img->width, img->height, img->stride[0], img->stride[1], img->stride[2]);

    return vb_blk; // return allocated block
}

void* sample_drawrec_proc(void* parg) {
    ot_vpss_grp grp = 0;
    ot_vpss_chn chn = 0;
    ot_video_frame_info frame_info;
    td_s32 milli_sec = 40;
    td_s32  ret;
    int policy;
    struct sched_param param;
    pthread_getschedparam(pthread_self(), &policy, &param);
    policy = SCHED_FIFO;
    param.sched_priority = sched_get_priority_min(policy);
    pthread_setschedparam(pthread_self(), policy, &param);

    stYolovDetectObjs* pOut = (stYolovDetectObjs*)malloc(sizeof(stYolovDetectObjs));
    while (nnn_thd_run) {
        ret = ss_mpi_vpss_get_chn_frame(grp, chn, &frame_info, milli_sec);
        if (ret != TD_SUCCESS) {
            continue;
        }
        pthread_mutex_lock(&algolock);
        memcpy(pOut, pDrawInfo, sizeof(stYolovDetectObjs));
        pthread_mutex_unlock(&algolock);

        float scale_x = 4000.0f / 640.0f;
        float scale_y = 3000.0f / 640.0f;
        for (size_t i = 0; i < pOut->count; i++) {
            pOut->objs[i].x = scale_x * pOut->objs[i].x;
            pOut->objs[i].y = scale_y * pOut->objs[i].y;
            pOut->objs[i].w = scale_x * pOut->objs[i].w;
            pOut->objs[i].h = scale_y * pOut->objs[i].h;
        }
        for (size_t i = 0; i < pOut->id_count; i++) {
            pOut->id_objs[i].x = scale_x * pOut->id_objs[i].x;
            pOut->id_objs[i].y = scale_y * pOut->id_objs[i].y;
            pOut->id_objs[i].w = scale_x * pOut->id_objs[i].w;
            pOut->id_objs[i].h = scale_y * pOut->id_objs[i].h;
            // printf("[%d] %d %d %d %d\n", i, pOut->id_objs[i].x, pOut->id_objs[i].y, pOut->id_objs[i].w, pOut->id_objs[i].h);
        }
        vgsdraw(&frame_info, pOut);
        DrawOsdInfo(pOut);
        ret = ss_mpi_vo_send_frame(0, 1, &frame_info, milli_sec);
        ss_mpi_vpss_release_chn_frame(grp, chn, &frame_info);

    }
    free(pOut);
    return NULL;
}

void* sample_nnnn_proc(void* parg) {
    ot_vpss_grp grp = 0;
    ot_vpss_chn chn = 1;
    ot_video_frame_info frame_info;
    td_s32 milli_sec = 40;
    td_s32  ret;
    printf("%s %d\n", __FUNCTION__, __LINE__);
    nnn_init();
#ifdef FEATURE_SORT
    nnn_feature_init();
    printf("nnn_feature_init success\n");
    deepsort_init();
#endif
    int policy;
    struct sched_param param;
    pthread_getschedparam(pthread_self(), &policy, &param);
    policy = SCHED_FIFO;
    param.sched_priority = sched_get_priority_max(policy);
    pthread_setschedparam(pthread_self(), policy, &param);

    stYolovDetectObjs* pOut = (stYolovDetectObjs*)malloc(sizeof(stYolovDetectObjs));
    ot_svp_img imgAlgo;
    // CreateUsrFrame(&imgAlgo, 640, 640);
    // CreateUsrFrame(&imgAlgo, 320, 320);
    ot_vb_blk vb_blk_algo = CreateRGBFrame(&imgAlgo, 128, 128); // for model input
    printf("640 resolution\n");

    int fps = 0;
    long prems = getms();

    while (nnn_thd_run) {
        ret = ss_mpi_vpss_get_chn_frame(grp, chn, &frame_info, milli_sec);
        if (ret != TD_SUCCESS) {
            continue;
        }
        
        int w = imgAlgo.width;
        int h = imgAlgo.height;
        int framelen = w * h * 3 / 2;
        long start = getms();
        // framecpy(&imgAlgo, &frame_info);

        // ==================crop====================
        // allocate cropImage(YUV420SP)
        td_s32 crop_x = 640;
        td_s32 crop_y = 640;
        td_s32 crop_w = 640;
        td_s32 crop_h = 640;
        ot_svp_img imgCrop;
        ot_vb_blk vb_blk_crop = CreateYUV420SPFrame(&imgCrop, crop_w, crop_h); // for crop
        frame_crop(&imgCrop, &frame_info, crop_x, crop_y);
        printf("cropp done\n");
        

        // ==================color conversion====================
        ot_svp_img imgRGB;
        ot_vb_blk vb_blk_rgb = CreateRGBFrame(&imgRGB, imgCrop.width, imgCrop.height); 
        frame_YUV420SP2RGB(&imgCrop, &imgRGB);
        printf("color conversion done\n");

        //===================resize=====================
        frame_resize(&imgRGB, &imgAlgo);
        printf("resizing done\n");

        save_yuv420sp("crop_output.yuv", &imgCrop); 
        save_rgb("crop_output.rgb", &imgRGB);
        save_rgb("resize_output.rgb", &imgAlgo);


        // nnn_execute((td_void*)imgAlgo.virt_addr[0], framelen, pOut);
        long nnntm = getms();
#ifdef FEATURE_SORT
        // get_boundbox_features(imgAlgo.virt_addr[0], framelen, pOut);
        // track_deepsort(pOut);
#endif
        // release resoucres for imgCrop
        ss_mpi_sys_munmap(imgCrop.virt_addr[0], imgCrop.width * imgCrop.height * 3 / 2);
        ss_mpi_vb_release_blk(vb_blk_crop); 

        // release resoucres for imgRGB
        ss_mpi_sys_munmap(imgRGB.virt_addr[0], imgRGB.width * imgRGB.height * 3);
        ss_mpi_vb_release_blk(vb_blk_rgb); 

        // write result 
        pthread_mutex_lock(&algolock);
        pDrawInfo->count = 0;
        pDrawInfo->id_count = 0;
        for (size_t i = 0; i < pOut->count; i++) {
            pDrawInfo->objs[i].x = pOut->objs[i].x;
            pDrawInfo->objs[i].y = pOut->objs[i].y;
            pDrawInfo->objs[i].w = pOut->objs[i].w;
            pDrawInfo->objs[i].h = pOut->objs[i].h;
        }

        for (size_t i = 0; i < pOut->id_count; i++) {
            pDrawInfo->id_objs[i].id = pOut->id_objs[i].id;
            pDrawInfo->id_objs[i].x = pOut->id_objs[i].x;
            pDrawInfo->id_objs[i].y = pOut->id_objs[i].y;
            pDrawInfo->id_objs[i].w = pOut->id_objs[i].w;
            pDrawInfo->id_objs[i].h = pOut->id_objs[i].h;
        }
        pDrawInfo->count = pOut->count;
        pDrawInfo->id_count = pOut->id_count;
        pthread_mutex_unlock(&algolock);

        long deepsort = getms();
        printf("execution time: %ld(nnn:%ld,deepsort:%ld) ms \n", getms() - start, nnntm - start, deepsort - nnntm);

        ss_mpi_vpss_release_chn_frame(grp, chn, &frame_info);
        fps++;
        if (start - prems >= 1000) {
            prems = start;
            printf("=====================================================  nnn  %d fps\n", fps);
            fps = 0;
        }
        nnn_thd_run = 0;  // debug
    }
    if (pOut) {
        free(pOut);
    }
    
    printf("%s %d\n", __FUNCTION__, __LINE__);
    return NULL;
}

static td_s32 sample_vio_second_sensor(td_void) {
    td_s32 ret;
    sample_sns_type sns_type;
    ot_vi_vpss_mode_type mode_type = OT_VI_OFFLINE_VPSS_OFFLINE;
    ot_vi_video_mode video_mode = OT_VI_VIDEO_MODE_NORM;
    const ot_vi_pipe vi_pipe = 0;
    const ot_vi_chn vi_chn = 0;
    ot_vpss_grp vpss_grp[1] = { 0 };
    const td_u32 grp_num = 1;
    const ot_vpss_chn vpss_chn = 0;
    sample_vi_cfg vi_cfg;
    ot_vi_dev vi_dev;
    ot_size in_size;

    sns_type = SENSOR0_TYPE;
    vi_dev = 2;

    ret = sample_vio_sys_init(mode_type, video_mode, VB_YUV_ROUTE_CNT, VB_LINEAR_RAW_CNT);
    if (ret != TD_SUCCESS) {
        goto sys_init_failed;
    }
    sns_type = SENSOR0_TYPE;
    sample_comm_vi_get_size_by_sns_type(sns_type, &in_size);


    (td_void)
        memset_s(&vi_cfg, sizeof(sample_vi_cfg), 0, sizeof(sample_vi_cfg));

    sample_comm_vi_get_default_vi_cfg(sns_type, &vi_cfg);
    vi_cfg.pipe_info[0].chn_info[0].chn_attr.depth = 2;
    vi_cfg.pipe_info[0].pipe_attr.compress_mode = OT_COMPRESS_MODE_NONE;

    sample_comm_vi_get_mipi_info_by_dev_id(sns_type, vi_dev,
                                             &vi_cfg.mipi_info);
    vi_cfg.sns_info.bus_id = 5; /* i2c5 */
    vi_cfg.sns_info.sns_clk_src = 1;
    vi_cfg.sns_info.sns_rst_src = 1;

    vi_cfg.dev_info.dev_attr.data_rate = OT_DATA_RATE_X1;

    vi_cfg.dev_info.vi_dev = vi_dev;

    ret = sample_comm_vi_start_vi(&vi_cfg);
    if (ret != TD_SUCCESS) {
      printf("sample_comm_vi_start_vi failed\n");
      goto start_vi_failed;
    }

    sample_comm_vi_bind_vpss(vi_pipe, vi_chn, vpss_grp[0], vpss_chn);

    ret = sample_vio_start_vpss(vpss_grp[0], &in_size);
    if (ret != TD_SUCCESS) {
        goto start_vpss_failed;
    }

    ret = sample_vio_start_venc_and_vo(vpss_grp, grp_num, &in_size);
    if (ret != TD_SUCCESS) {
        goto start_venc_and_vo_failed;
    }

    nnn_thd_run = 1;
    pDrawInfo = (stYolovDetectObjs*)malloc(sizeof(stYolovDetectObjs));
    pDrawInfo->count = 0;
    pthread_create(&nnn_pid, 0, sample_nnnn_proc, NULL);
    pthread_create(&vodrawrc_pid, 0, sample_drawrec_proc, NULL);
    UiAppMain();

    sample_get_char();
    UiAppMainStop();
    nnn_thd_run = 0;
    pthread_join(nnn_pid, 0);
    pthread_join(vodrawrc_pid, 0);
    free(pDrawInfo);

    sample_vio_stop_venc_and_vo(vpss_grp, grp_num);

start_venc_and_vo_failed:
    sample_vio_stop_vpss(vpss_grp[0]);
start_vpss_failed:
    sample_comm_vi_un_bind_vpss(vi_pipe, vi_chn, vpss_grp[0], vpss_chn);
    sample_comm_vi_stop_vi(&vi_cfg);
 
start_vi_failed:
    sample_comm_sys_exit();
sys_init_failed:
    return ret;
}

static td_s32 sample_vio_all(td_void) {
    td_s32 ret;
    ot_vi_vpss_mode_type mode_type = OT_VI_OFFLINE_VPSS_OFFLINE;
    ot_vi_video_mode video_mode = OT_VI_VIDEO_MODE_NORM;
    const ot_vi_pipe vi_pipe = 0;
    const ot_vi_chn vi_chn = 0;
    ot_vpss_grp vpss_grp[1] = { 0 };
    const td_u32 grp_num = 1;
    const ot_vpss_chn vpss_chn = 0;
    sample_vi_cfg vi_cfg;
    sample_sns_type sns_type;
    ot_size in_size;

    ret = sample_vio_sys_init(mode_type, video_mode, VB_YUV_ROUTE_CNT, VB_LINEAR_RAW_CNT);
    if (ret != TD_SUCCESS) {
        goto sys_init_failed;
    }
    sns_type = SENSOR0_TYPE;
    sample_comm_vi_get_size_by_sns_type(sns_type, &in_size);
    printf("sensor intput height: %d\n", in_size.height);
    printf("sensor intput width: %d\n", in_size.width);


    sample_comm_vi_get_default_vi_cfg(sns_type, &vi_cfg);
    ret = sample_comm_vi_start_vi(&vi_cfg);
    if (ret != TD_SUCCESS) {
        goto start_vi_failed;
    }
    sample_comm_vi_bind_vpss(vi_pipe, vi_chn, vpss_grp[0], vpss_chn);

    ret = sample_vio_start_vpss(vpss_grp[0], &in_size);
    if (ret != TD_SUCCESS) {
        goto start_vpss_failed;
    }

    ret = sample_vio_start_venc_and_vo(vpss_grp, grp_num, &in_size);
    if (ret != TD_SUCCESS) {
        goto start_venc_and_vo_failed;
    }

    nnn_thd_run = 1;
    pDrawInfo = (stYolovDetectObjs*)malloc(sizeof(stYolovDetectObjs));
    pDrawInfo->count = 0;
    pthread_create(&nnn_pid, 0, sample_nnnn_proc, NULL);
    // pthread_create(&vodrawrc_pid, 0, sample_drawrec_proc, NULL);
    UiAppMain();

    sample_get_char();
    UiAppMainStop();
    nnn_thd_run = 0;
    pthread_join(nnn_pid, 0);
    // pthread_join(vodrawrc_pid, 0);
    free(pDrawInfo);

    sample_vio_stop_venc_and_vo(vpss_grp, grp_num);

start_venc_and_vo_failed:
    sample_vio_stop_vpss(vpss_grp[0]);
start_vpss_failed:
    sample_comm_vi_un_bind_vpss(vi_pipe, vi_chn, vpss_grp[0], vpss_chn);
    sample_comm_vi_stop_vi(&vi_cfg);
 
start_vi_failed:
    sample_comm_sys_exit();
sys_init_failed:
    return ret;
}

static td_void sample_vio_handle_sig(td_s32 signo) {
    if (signo == SIGINT || signo == SIGTERM) {
        g_sig_flag = 1;
    }
}

static td_void sample_register_sig_handler(td_void(*sig_handle)(td_s32)) {
    struct sigaction sa;

    (td_void)memset_s(&sa, sizeof(struct sigaction), 0, sizeof(struct sigaction));
    sa.sa_handler = sig_handle;
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, TD_NULL);
    sigaction(SIGTERM, &sa, TD_NULL);
}

td_s32 main(td_s32 argc, td_char* argv[]) {
    td_s32 ret;
    sample_register_sig_handler(sample_vio_handle_sig);
    ret = sample_vio_all();
    // ret = sample_vio_second_sensor();
    if ((ret == TD_SUCCESS) && (g_sig_flag == 0)) {
        printf("\033[0;32mprogram exit normally!\033[0;39m\n");
    }
    else {
        printf("\033[0;31mprogram exit abnormally!\033[0;39m\n");
    }
    exit(ret);
}


