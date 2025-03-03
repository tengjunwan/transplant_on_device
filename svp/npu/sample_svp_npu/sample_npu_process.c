/*
  Copyright (c), 2001-2022, Shenshu Tech. Co., Ltd.
 */

#include "sample_npu_process.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>
#include <pthread.h>
#include <limits.h>

#include "ot_common_svp.h"
#include "sample_common_svp.h"
#include "sample_npu_model.h"
#include "detectobjs.h"

static td_u32 g_npu_dev_id = 0;


static td_s32 stmtrack_init_resource(td_void)
{   // TODO: add context and stream
    /* ACL init */
    const char *acl_config_path = "";
    aclrtRunMode run_mode;
    td_s32 ret;

    ret = aclInit(acl_config_path);
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("acl init fail.\n");
        return TD_FAILURE;
    }
    sample_svp_trace_info("acl init success.\n");

    /* open device */
    ret = aclrtSetDevice(g_npu_dev_id);
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("acl open device %d fail.\n", g_npu_dev_id);
        return TD_FAILURE;
    }
    sample_svp_trace_info("open device %d success.\n", g_npu_dev_id);

    /* get run mode */
    ret = aclrtGetRunMode(&run_mode);
    if ((ret != ACL_ERROR_NONE) || (run_mode != ACL_DEVICE)) {
        sample_svp_trace_err("acl get run mode fail.\n");
        return TD_FAILURE;
    }
    sample_svp_trace_info("get run mode success\n");

    return TD_SUCCESS;
}

static td_void stmtrack_destroy_resource(td_void)
{   // TODO: add context and stream 
    aclError ret;
    ret = aclrtResetDevice(g_npu_dev_id);
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("reset device fail\n");
    }
    sample_svp_trace_info("end to reset device is %d\n", g_npu_dev_id);

    ret = aclFinalize();
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("finalize acl fail\n");
    }
    sample_svp_trace_info("end to finalize acl\n");
}

static td_void stmtrack_loading_stage_cleanup(td_u32 model_num)
{   //
    for (td_u32 model_index = 0; model_index < model_num; model_index++) {
        sample_npu_destroy_desc(model_index);
        sample_npu_unload_model(model_index);
    }
    stmtrack_destroy_resource();
}


static td_s32 sample_svp_npu_load_model(const char* om_model_path, td_u32 model_index, td_bool is_cached); // prototype


td_s32 stmtrack_prepare_init_resources()
{   // do all the preparations: acl init, model_loading, buffer allocate, dataset create since it's static inference
    td_s32 ret;

    // ===================acl init===================
    ret = stmtrack_init_resource();
    if (ret != TD_SUCCESS) {
        stmtrack_destroy_resource();
        return ret;
    }

    // ===================load models==================
    // load query model
    const char *om_model_path_query = "STMTrack_FeatureExtractionQuery.om";  
    printf("start loading model: %s\n", om_model_path_query);  
    ret = sample_svp_npu_load_model(om_model_path_query, 0, TD_FALSE); // load model, create model desc; model index = '0'
    if (ret != TD_SUCCESS) {
        printf("failed to load model: : %s\n", om_model_path_query);
        goto acl_process_end0;
    }

    // load memory model
    const char *om_model_path_memory = "STMTrack_FeatureExtractionMemory.om";  
    printf("start loading model: %s\n", om_model_path_memory);  
    ret = sample_svp_npu_load_model(om_model_path_memory, 1, TD_FALSE);  // model index = '1'
    if (ret != TD_SUCCESS) {
        printf("failed to load model: : %s\n", om_model_path_memory);
        goto acl_process_end0;
    }

    // load readMemoryAndHead model
    const char *om_model_path_readMemoryAndHead = "STMTrack_ReadMemoryAndHead.om";  
    // const char *om_model_path_readMemoryAndHead = "STMTrack_FeatureExtractionMemory.om";  
    printf("start loading model: %s\n", om_model_path_readMemoryAndHead);  
    ret = sample_svp_npu_load_model(om_model_path_readMemoryAndHead, 2, TD_FALSE); // model index = '2'
    if (ret != TD_SUCCESS) {
        printf("failed to load model: : %s\n", om_model_path_readMemoryAndHead);
        goto acl_process_end0;
    }

    return 0;


acl_process_end0:
    stmtrack_loading_stage_cleanup(3);  // unload model, destroy model desc, (TODO stream, context), reset device, acl finalize, '3' means we have 3 models
    return ret;
}



static td_s32 sample_svp_npu_dataset_prepare_init(td_u32 model_index); // prototype
static td_s32 sample_svp_npu_create_input_databuf(td_void *data_buf, size_t data_len, td_u32 model_index); // prototype

td_s32 stmtrack_execute(td_void* query_buf, size_t query_len, td_void* memory_buf, size_t memory_len, 
                        td_void* mask_buf, size_t mask_len)
{
    // model_index=0: query model
    // model_index=1: memory model
    // model_index=2: readMemoryAndHead model
    td_s32 ret;
    // prepare dataset
    for (td_u32 i = 0; i < 3; i++) {
        ret = sample_svp_npu_dataset_prepare_init(i); // create input dataset(no input data buffer allocated or created) 
                                                      // create ouput dataset, allocate and create output buffer('aclrtMalloc' is called)                                                        
    }
    // create databuffer for readMemoryAndHead
    // ret = sample_svp_npu_dataset_prepare_initV2(2);


    ret = sample_svp_npu_create_input_databuf(query_buf, query_len, 0);  // use query buffer to create data buffer for query model
    if (ret != TD_SUCCESS) {
        sample_svp_trace_err("query: memcpy_s device buffer fail.\n");
        return -1;
    }
    ret = sample_npu_create_input_databufV2(memory_buf, memory_len, mask_buf, mask_len, 1);  // use memory buffer & mask buffer to create data buffer for memory model
    if (ret != TD_SUCCESS) {
        sample_svp_trace_err("memory memcpy_s device buffer fail.\n");
        return -1;
    }
    
    // run query model
    ret = sample_npu_model_execute(0);
    if (ret != TD_SUCCESS) {
        sample_svp_trace_err("memcpy_s device buffer fail.\n");
        return -1;
    }
    printf("query model inference done;\n");

    // run memory model
    ret = sample_npu_model_execute(1);
    if (ret != TD_SUCCESS) {
        sample_svp_trace_err("memcpy_s device buffer fail.\n");
        return -1;
    }
    printf("memory model inference done;\n");

    // link output databuffer of query & memory model to inputdataset of readMemoryAndHead
    sample_npu_model_link_buffer(0, 1, 2); 
    // create_dummy_inputdatabuffer(2);
    printf("link done\n");

    // run readMemoryAndHead model
    ret = sample_npu_model_execute(2);
    if (ret != TD_SUCCESS) {
        sample_svp_trace_err("memcpy_s device buffer fail.\n");
        return -1;
    }
    printf("readMemoryAndHead model inference done;\n");
    // output final output
    // sample_npu_output_model_result_memory(model_index);

    // for consistency debug
    sample_npu_output_model_result_query(0);
    sample_npu_output_model_result_memory(1);
    sample_npu_output_model_input_head(2);
    sample_npu_output_model_result_head(2);

    // sample_npu_output_model_result(0, pOut);
    // destroy all datasets and databuffers for all models
    for (td_u32 i = 0; i < 3; i++) {
        sample_npu_destroy_output(i); // release output databuffer and destory output dataset
        sample_npu_destroy_input_dataset(i); // only destroy input dataset
    }
    
    return 0;
}

// create full dataset, include databuffer and allocate space for databuffer, similar to "sample_svp_npu_dataset_prepare_init"
static td_s32 sample_svp_npu_dataset_prepare_initV2(td_u32 model_index)
{
    td_s32 ret;

    ret = sample_npu_create_input(model_index);
    if (ret != TD_SUCCESS) {
        sample_svp_trace_err("execute create input fail.\n");
        return TD_FAILURE;
    }
    ret = sample_npu_create_output(model_index);
    if (ret != TD_SUCCESS) {
        sample_npu_destroy_input(model_index);
        sample_svp_trace_err("execute create output fail.\n");
        return TD_FAILURE;
    }
    return TD_SUCCESS;
}



// ===================================below is reference code=============================================

static td_void sample_svp_npu_destroy_resource(td_void)
{
    aclError ret;

    ret = aclrtResetDevice(g_npu_dev_id);
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("reset device fail\n");
    }
    sample_svp_trace_info("end to reset device is %d\n", g_npu_dev_id);

    ret = aclFinalize();
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("finalize acl fail\n");
    }
    sample_svp_trace_info("end to finalize acl\n");
}

static td_s32 sample_svp_npu_init_resource(td_void)
{
    /* ACL init */
    const char *acl_config_path = "";
    aclrtRunMode run_mode;
    td_s32 ret;

    ret = aclInit(acl_config_path);
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("acl init fail.\n");
        return TD_FAILURE;
    }
    sample_svp_trace_info("acl init success.\n");

    /* open device */
    ret = aclrtSetDevice(g_npu_dev_id);
    if (ret != ACL_ERROR_NONE) {
        sample_svp_trace_err("acl open device %d fail.\n", g_npu_dev_id);
        return TD_FAILURE;
    }
    sample_svp_trace_info("open device %d success.\n", g_npu_dev_id);

    /* get run mode */
    ret = aclrtGetRunMode(&run_mode);
    if ((ret != ACL_ERROR_NONE) || (run_mode != ACL_DEVICE)) {
        sample_svp_trace_err("acl get run mode fail.\n");
        return TD_FAILURE;
    }
    sample_svp_trace_info("get run mode success\n");

    return TD_SUCCESS;
}


static td_s32 sample_svp_npu_acl_prepare_init()
{
    td_s32 ret;

    ret = sample_svp_npu_init_resource();
    if (ret != TD_SUCCESS) {
        sample_svp_npu_destroy_resource();
    }

    return ret;
}

static td_void sample_svp_npu_acl_prepare_exit(td_u32 thread_num)
{
    for (td_u32 model_index = 0; model_index < thread_num; model_index++) {
        sample_npu_destroy_desc(model_index);
        sample_npu_unload_model(model_index);
    }
    sample_svp_npu_destroy_resource();
}

static td_s32 sample_svp_npu_load_model(const char* om_model_path, td_u32 model_index, td_bool is_cached)
{
    td_char path[PATH_MAX] = { 0 };
    td_s32 ret;

    if (sizeof(om_model_path) > PATH_MAX) {
        sample_svp_trace_err("pathname too long!.\n");
        return TD_NULL;
    }
    if (realpath(om_model_path, path) == TD_NULL) {
        sample_svp_trace_err("invalid file!.\n");
        return TD_NULL;
    }

    if (is_cached == TD_TRUE) {
        ret = sample_npu_load_model_with_mem_cached(path, model_index);
    } else {
        ret = sample_npu_load_model_with_mem(path, model_index);
    }

    if (ret != TD_SUCCESS) {
        sample_svp_trace_err("execute load model fail, model_index is:%d.\n", model_index);
        goto acl_prepare_end1;
    }
    ret = sample_npu_create_desc(model_index);
    if (ret != TD_SUCCESS) {
        sample_svp_trace_err("execute create desc fail.\n");
        goto acl_prepare_end2;
    }
    printModelIODescription(model_index);

    return TD_SUCCESS;

acl_prepare_end2:
    sample_npu_destroy_desc(model_index);

acl_prepare_end1:
    sample_npu_unload_model(model_index);
    return ret;
}

static td_s32 sample_svp_npu_dataset_prepare_init(td_u32 model_index)
{
    td_s32 ret;

    ret = sample_npu_create_input_dataset(model_index);
    if (ret != TD_SUCCESS) {
        sample_svp_trace_err("execute create input fail.\n");
        return TD_FAILURE;
    }
    ret = sample_npu_create_output(model_index);
    if (ret != TD_SUCCESS) {
        sample_npu_destroy_input_dataset(model_index);
        sample_svp_trace_err("execute create output fail.\n");
        return TD_FAILURE;
    }
    return TD_SUCCESS;
}

static td_s32 sample_svp_npu_create_input_databuf(td_void *data_buf, size_t data_len, td_u32 model_index)
{
    return sample_npu_create_input_databuf(data_buf, data_len, model_index);
}

static td_void sample_svp_npu_destroy_input_databuf(td_u32 thread_num)
{
    for (td_u32 model_index = 0; model_index < thread_num; model_index++) {
        sample_npu_destroy_input_databuf(model_index);
    }
}

/* function : show the sample of npu yolov8n */
td_void nnn_init(td_void) {
    td_s32 ret;

    // const char *om_model_path = "yolov8n.om"; 
    const char *om_model_path = "STMTrack_FeatureExtractionQuery.om";  
    printf("start loading model: %s\n", om_model_path);   
    ret = sample_svp_npu_acl_prepare_init(om_model_path);
    if (ret != TD_SUCCESS) {
        return;
    }

    ret = sample_svp_npu_load_model(om_model_path, 0, TD_FALSE);
    if (ret != TD_SUCCESS) {
        goto acl_process_end0;
    }

   return ;
acl_process_end0:
    sample_svp_npu_acl_prepare_exit(1);
}


td_void stmtrack_init(td_void)
{
    td_s32 ret;
    // const char *om_model_path = "STMTrack_FeatureExtractionMemory.om";    
    ret = sample_svp_npu_acl_prepare_init(); // acl init, set device, (TODO: create context, create stream)

    // load query model
    const char *om_model_path_query = "STMTrack_FeatureExtractionQuery.om";  
    printf("start loading model: %s\n", om_model_path_query);  
    ret = sample_svp_npu_load_model(om_model_path_query, 0, TD_FALSE); // load model, create model desc; model index = '0'
    if (ret != TD_SUCCESS) {
        printf("failed to load model: : %s\n", om_model_path_query);
        goto acl_process_end0;
    }

    // load memory model
    const char *om_model_path_memory = "STMTrack_FeatureExtractionMemory.om";  
    printf("start loading model: %s\n", om_model_path_memory);  
    ret = sample_svp_npu_load_model(om_model_path_memory, 1, TD_FALSE);  // model index = '1'
    if (ret != TD_SUCCESS) {
        printf("failed to load model: : %s\n", om_model_path_memory);
        goto acl_process_end0;
    }

    // load readMemoryAndHead model
    const char *om_model_path_readMemoryAndHead = "STMTrack_ReadMemoryAndHead.om";  
    printf("start loading model: %s\n", om_model_path_readMemoryAndHead);  
    ret = sample_svp_npu_load_model(om_model_path_readMemoryAndHead, 2, TD_FALSE); // model index = '2'
    if (ret != TD_SUCCESS) {
        printf("failed to load model: : %s\n", om_model_path_readMemoryAndHead);
        goto acl_process_end0;
    }

    return 0;


acl_process_end0:
    sample_svp_npu_acl_prepare_exit(3);  // unload model, destroy model desc, (TODO stream, context), reset device, acl finalize, '3' means we have 3 models
}


td_void nnn_feature_init(td_void)
{
    td_s32 ret;
    // const char *om_model_path = "STMTrack_FeatureExtractionMemory.om";    
    const char *om_model_path = "STMTrack_FeatureExtractionQuery.om";  
    ret = sample_svp_npu_load_model(om_model_path, 1, TD_FALSE);
    if (ret != TD_SUCCESS) {
        goto acl_process_end0;
    }
   return 0;
acl_process_end0:

    sample_svp_npu_acl_prepare_exit(1);
}

td_s32 nnn_execute(td_void* data_buf, size_t data_len,stYolovDetectObjs* pOut)
{
    td_s32 ret;
    ret = sample_svp_npu_dataset_prepare_init(0);

    ret = sample_svp_npu_create_input_databuf(data_buf, data_len, 0);
    if (ret != TD_SUCCESS) {
        sample_svp_trace_err("memcpy_s device buffer fail.\n");
        return -1;
    }
    ret = sample_npu_model_execute(0);
    if (ret != TD_SUCCESS) {
        sample_svp_trace_err("execute inference fail.\n");
        return -1;
    }
    printf("model inference done;");
    // sample_npu_output_model_result(0, pOut);
    sample_npu_destroy_input_databuf(0);
    sample_npu_destroy_output(0);
    sample_npu_destroy_input_dataset(0);
    return 0;
}



td_s32 query_execute(td_void* data_buf, size_t data_len)
{
    td_s32 ret;
    td_u32 model_index = 0;
    ret = sample_svp_npu_dataset_prepare_init(model_index); // create input dataset(no input data buffer allocated or created) 
                                                  // create ouput dataset, allocate and create output buffer('aclrtMalloc' is called)

    ret = sample_svp_npu_create_input_databuf(data_buf, data_len, model_index);  // create data buffer with 'data_buf'(already allocated, no need to call 'aclrtMalloc')
    if (ret != TD_SUCCESS) {
        sample_svp_trace_err("memcpy_s device buffer fail.\n");
        return -1;
    }
    ret = sample_npu_model_execute(model_index); // wrapper of 'aclmdExecute'
    if (ret != TD_SUCCESS) {
        sample_svp_trace_err("execute inference fail.\n");
        return -1;
    }
    printf("query model inference done;\n");
    // sample_npu_output_model_result(0, pOut);
    sample_npu_output_model_result_query(model_index);
    sample_npu_destroy_input_databuf(model_index);
    sample_npu_destroy_output(model_index);
    sample_npu_destroy_input_dataset(model_index);
    return 0;
}

td_s32 memory_execute(td_void* data1_buf, size_t data1_len, td_void* data2_buf, size_t data2_len)
{
    td_s32 ret;
    td_u32 model_index = 1;
    ret = sample_svp_npu_dataset_prepare_init(model_index); // create input dataset(no input data buffer allocated or created) 
                                                  // create ouput dataset, allocate and create output buffer('aclrtMalloc' is called)
                                                  // '0' means model index

    ret = sample_npu_create_input_databufV2(data1_buf, data1_len, data2_buf, data2_len, model_index);  // create data buffer with 'data_buf'(already allocated, 
                                                                                             // no need to call 'aclrtMalloc')
    if (ret != TD_SUCCESS) {
        sample_svp_trace_err("memcpy_s device buffer fail.\n");
        return -1;
    }
    ret = sample_npu_model_execute(model_index); // wrapper of 'aclmdExecute'
    if (ret != TD_SUCCESS) {
        sample_svp_trace_err("execute inference fail.\n");
        return -1;
    }
    printf("memory model inference done;\n");
    // sample_npu_output_model_result(0, pOut);
    sample_npu_output_model_result_memory(model_index);
    sample_npu_destroy_input_databuf(model_index);
    sample_npu_destroy_output(model_index);
    sample_npu_destroy_input_dataset(model_index);
    return 0;
}

// td_void nnn_feature_init(td_void)
// {
//     td_s32 ret;
//     // const char *om_model_path = "STMTrack_FeatureExtractionMemory.om";    
//     const char *om_model_path = "STMTrack_FeatureExtractionQuery.om";  
//     ret = sample_svp_npu_load_model(om_model_path, 1, TD_FALSE);
//     if (ret != TD_SUCCESS) {
//         goto acl_process_end0;
//     }
//    return 0;
// acl_process_end0:

//     sample_svp_npu_acl_prepare_exit(1);
// }

td_s32 nnn_feature_execute(td_void* data_buf, size_t data_len, stObjinfo* p_objs)
{
    td_s32 ret;
    td_u32 model_index = 1;
    ret = sample_svp_npu_dataset_prepare_init(model_index);

    ret = sample_svp_npu_create_input_databuf(data_buf, data_len, model_index); // h+c 直接给地址
    if (ret != TD_SUCCESS) {
        sample_svp_trace_err("memcpy_s device buffer fail.\n");
        return -1;
    }
    ret = sample_npu_model_execute(model_index);
    if (ret != TD_SUCCESS) {
        sample_svp_trace_err("execute inference fail.\n");
        return -1;
    }

    sample_npu_FEATURE_output_model_result(model_index, p_objs);

    sample_npu_destroy_input_databuf(model_index);
    sample_npu_destroy_output(model_index);
    sample_npu_destroy_input_dataset(model_index);
    return 0;
}

typedef struct {
    int width;
    int height;
} Dimension;

typedef struct {
    int x;
    int y;
    Dimension size;
} CropArea;

/*
    src: 输入的YUV420SP buffer
    srcDim: 输入的YUV420SP数据的尺寸
    dst: 输出的YUV420SP buffer
    newDim: 输出的YUV420SP数据的尺寸
    cropArea: 裁剪区域
    onlyCrop_flag: 是否只进行裁剪，如果为1，则只进行裁剪，不进行缩放， 这时newDim就不会被用到。
*/
void cropAndScaleYUV420SP(unsigned char* src, Dimension srcDim,
    unsigned char* dst, Dimension newDim,
    CropArea cropArea, int onlyCrop_flag) {
    // 计算目标尺寸
    Dimension targetDim = onlyCrop_flag ? cropArea.size : newDim;

    // 裁剪并缩放Y分量
    for (int y = 0; y < targetDim.height; ++y) {
        for (int x = 0; x < targetDim.width; ++x) {
            int srcX = cropArea.x + (x * cropArea.size.width) / targetDim.width;
            int srcY = cropArea.y + (y * cropArea.size.height) / targetDim.height;
            dst[y * targetDim.width + x] = src[srcY * srcDim.width + srcX];
        }
    }

    // 裁剪并缩放UV分量
    int uvSrcWidth = srcDim.width / 2;
    int uvSrcHeight = srcDim.height / 2;
    int uvTargetWidth = targetDim.width / 2;
    int uvTargetHeight = targetDim.height / 2;
    int uvCropX = cropArea.x / 2;
    int uvCropY = cropArea.y / 2;
    int uvCropWidth = cropArea.size.width / 2;
    int uvCropHeight = cropArea.size.height / 2;

    unsigned char* srcUV = src + srcDim.width * srcDim.height;
    unsigned char* dstUV = dst + targetDim.width * targetDim.height;

    for (int y = 0; y < uvTargetHeight; ++y) {
        for (int x = 0; x < uvTargetWidth; ++x) {
            int srcX = uvCropX + (x * uvCropWidth) / uvTargetWidth;
            int srcY = uvCropY + (y * uvCropHeight) / uvTargetHeight;
            int srcIndex = (srcY * uvSrcWidth + srcX) * 2; // 2 bytes per UV pair
            int dstIndex = (y * uvTargetWidth + x) * 2; // 2 bytes per UV pair
            dstUV[dstIndex] = srcUV[srcIndex];     // U
            dstUV[dstIndex + 1] = srcUV[srcIndex + 1]; // V
        }
    }
}

#define YUV_WIDTH 640
#define YUV_HEIGHT 640
#define DEST_WIDTH 64
#define DEST_HEIGHT 128

int save_boundbox_yuv(char *pic_zone, int zone_framelen) {
    static int index_tmp_file = 0;
    index_tmp_file++;
    if (index_tmp_file > 100 && index_tmp_file < 110) {
        char fileName[20];
        sprintf(fileName, "crop%d.yuv420", index_tmp_file);
        FILE* fp = fopen(fileName, "wb");
        if (fp == NULL) {
            printf("Failed to open file %s\n", fileName);
            return -1;
        }
        if (fwrite(pic_zone, 1, zone_framelen, fp) != zone_framelen) {
            printf("Failed to write file %s\n", fileName);
            fclose(fp);
            return -1;
        }
        fclose(fp);
    }
    return 0;
}

/*
    data_buf: 输入的YUV420SP buffer, 640x640
    data_len: 输入的YUV420SP buffer的长度
    pOut: 输入的Yolov5的检测结果
*/
void get_boundbox_features(td_void* data_buf, size_t data_len, stYolovDetectObjs* pOut)
{
    char pic_zone[DEST_WIDTH * DEST_HEIGHT * 3 / 2];
    Dimension srcDim = { YUV_WIDTH, YUV_HEIGHT };
    Dimension newDim = { DEST_WIDTH, DEST_HEIGHT };
    int zone_framelen = DEST_WIDTH * DEST_HEIGHT * 3 / 2;
    stObjinfo* pObj;
    long start_time_t, end_time_t;
    long start_time, end_time;

    start_time_t = getms();

    for (int i = 0; i < pOut->count; i++) {
        pObj = &pOut->objs[i];
        CropArea cropArea = { pObj->x, pObj->y, {pObj->w, pObj->h} };
        start_time = getms();
        cropAndScaleYUV420SP(data_buf, srcDim, pic_zone, newDim, cropArea, 0);
        end_time = getms();
        // printf("cropAndScaleYUV420SP execution time: %ld ms\n", end_time - start_time);
#if 0
        save_boundbox_yuv(pic_zone, zone_framelen);
#endif
        start_time = getms();
        nnn_feature_execute(pic_zone, zone_framelen, pObj);
        end_time = getms();
        // printf("nnn_feature_execute execution time: %ld ms\n", end_time - start_time);
    }

    end_time_t = getms();
    // printf("get_boundbox_features() execution time: %ld ms\n", end_time_t - start_time_t);
}



void resizeRGBBuffer(unsigned char* src, int srcWidth, int srcHeight,
    unsigned char* dst, int dstWidth, int dstHeight) {
    // 计算目标尺寸

    // number of pixels in a single channle
    int src_num_planar = srcWidth * srcHeight;
    int dst_num_planar = dstWidth * dstHeight;
    // scale ratios
    float scaleX = (float)srcWidth / dstWidth;
    float scaleY = (float)srcHeight / dstHeight;
    // 裁剪并缩放Y分量
    for (int c = 0; c < 3; ++c) {
        unsigned char* srcChannel = src + c * src_num_planar;
        unsigned char* dstChannel = dst + c * dst_num_planar;
        for (int y = 0; y < dstHeight; ++y) {
            for (int x = 0; x < srcWidth; ++x) {
                int srcX = (int)(x * scaleX);
                int srcY = (int)(y * scaleY);
                dstChannel[y * dstWidth + x] = srcChannel[srcY * srcWidth + srcX];
            }
        }
    }
}