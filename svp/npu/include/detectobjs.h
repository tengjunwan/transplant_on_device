#ifndef __DETECTOBJS_H__
#define __DETECTOBJS_H__

#ifdef __cplusplus
extern "C" {
#endif

#define OBJDETECTMAX 64
#define FEATURE_DIM 512

  typedef struct {
    int id;
    int x;
    int y;
    int w;
    int h;
  } IDRECT_t;

  typedef struct {
    int x;
    int y;
    int w;
    int h;
    int label;
    float score;
    float feature[FEATURE_DIM];   // add feature
  } stObjinfo;


  typedef struct {
    int count;
    stObjinfo objs[OBJDETECTMAX];
    int id_count;
    IDRECT_t id_objs[OBJDETECTMAX];
  } stYolovDetectObjs;


  // object tracing stuff
  typedef struct {
    float cx; // center x of bbox
    float cy; // center y of bbox
    float w;
    float h;
    float score; // score after post-processing
    float scale; // scale from source to 289*289
  } stmTrackerState;


#ifdef __cplusplus
}
#endif
#endif
