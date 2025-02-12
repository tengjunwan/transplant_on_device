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


#ifdef __cplusplus
}
#endif
#endif
