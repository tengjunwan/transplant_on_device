
#include "wrapperncnn.h"

#include "layer.h"
#include "net.h"

#if defined(USE_NCNN_SIMPLEOCV)
#include "simpleocv.h"
#else
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#endif
#include <float.h>
#include <stdio.h>
#include <vector>

struct Object {
  cv::Rect_<float> rect;
  int label;
  float prob;
};

static inline float intersection_area(const Object &a, const Object &b) {
  cv::Rect_<float> inter = a.rect & b.rect;
  return inter.area();
}

static void qsort_descent_inplace(std::vector<Object> &faceobjects, int left,
                                  int right) {
  int i = left;
  int j = right;
  float p = faceobjects[(left + right) / 2].prob;

  while (i <= j) {
    while (faceobjects[i].prob > p)
      i++;

    while (faceobjects[j].prob < p)
      j--;

    if (i <= j) {
      // swap
      std::swap(faceobjects[i], faceobjects[j]);

      i++;
      j--;
    }
  }

#pragma omp parallel sections
  {
#pragma omp section
    {
      if (left < j)
        qsort_descent_inplace(faceobjects, left, j);
    }
#pragma omp section
    {
      if (i < right)
        qsort_descent_inplace(faceobjects, i, right);
    }
  }
}

static void qsort_descent_inplace(std::vector<Object> &faceobjects) {
  if (faceobjects.empty())
    return;

  qsort_descent_inplace(faceobjects, 0, faceobjects.size() - 1);
}

static void nms_sorted_bboxes(const std::vector<Object> &faceobjects,
                              std::vector<int> &picked, float nms_threshold,
                              bool agnostic = false) {
  picked.clear();

  const int n = faceobjects.size();

  std::vector<float> areas(n);
  for (int i = 0; i < n; i++) {
    areas[i] = faceobjects[i].rect.area();
  }

  for (int i = 0; i < n; i++) {
    const Object &a = faceobjects[i];

    int keep = 1;
    for (int j = 0; j < (int)picked.size(); j++) {
      const Object &b = faceobjects[picked[j]];

      if (!agnostic && a.label != b.label)
        continue;

      // intersection over union
      float inter_area = intersection_area(a, b);
      float union_area = areas[i] + areas[picked[j]] - inter_area;
      // float IoU = inter_area / union_area
      if (inter_area / union_area > nms_threshold)
        keep = 0;
    }

    if (keep)
      picked.push_back(i);
  }
}

static void draw_objects(
    const std::vector<Object> &objects) {
  static const char *class_names[] = {
      "person", "bicycle", "car",
      "motorcycle", "airplane", "bus",
      "train", "truck", "boat",
      "traffic light", "fire hydrant", "stop sign",
      "parking meter", "bench", "bird",
      "cat", "dog", "horse",
      "sheep", "cow", "elephant",
      "bear", "zebra", "giraffe",
      "backpack", "umbrella", "handbag",
      "tie", "suitcase", "frisbee",
      "skis", "snowboard", "sports ball",
      "kite", "baseball bat", "baseball glove",
      "skateboard", "surfboard", "tennis racket",
      "bottle", "wine glass", "cup",
      "fork", "knife", "spoon",
      "bowl", "banana", "apple",
      "sandwich", "orange", "broccoli",
      "carrot", "hot dog", "pizza",
      "donut", "cake", "chair",
      "couch", "potted plant", "bed",
      "dining table", "toilet", "tv",
      "laptop", "mouse", "remote",
      "keyboard", "cell phone", "microwave",
      "oven", "toaster", "sink",
      "refrigerator", "book", "clock",
      "vase", "scissors", "teddy bear",
      "hair drier", "toothbrush"};

  for (size_t i = 0; i < objects.size(); i++) {
    const Object &obj = objects[i];
    char text[256];
    sprintf(text, "%s %.1f%%", class_names[obj.label], obj.prob * 100);
    printf("%d = %.5f at %.2f %.2f %.2f x %.2f %s \n", obj.label, obj.prob,
           obj.rect.x, obj.rect.y, obj.rect.width, obj.rect.height, text);
  }
}

static std::vector<Object> objects;
static int img_w = 640;
static int img_h = 640;
static float scale = 1.f;
static int wpad = 0;
static int hpad = 0;
static std::vector<Object> proposals;
static ncnn::Mat in_pad;
static int gcSize = 80; //模型需要识别的个数 如果模型全部进行识别则为80 但速度慢
static int detcect_label = 0;

int ncnn_result_yolov8(float *src, unsigned int len, stYolovDetectObjs *pOut) {
  pOut->count = 0;
  if (len < (gcSize + 4) * 8400) {
    return 0;
  }

  proposals.clear();

  float *pScore = NULL;
  const float prob_threshold = 0.45f;
  int cx, cy, cw, ch;
  for (size_t j = 0; j < gcSize; j++) {
    if (j == detcect_label) {
      for (size_t i = 0; i < 8400; i++) {
        /* code */
        pScore = src + ((j + 4) * 8400 + i);
        if (*pScore > prob_threshold) {
          Object obj;
          cx = src[i];
          cy = src[1 * 8400 + i];
          cw = src[2 * 8400 + i];
          ch = src[3 * 8400 + i];
          obj.rect.x = cx - cw / 2;
          obj.rect.y = cy - ch / 2;
          obj.rect.width = cw;
          obj.rect.height = ch;
          obj.label = j;
          obj.prob = *pScore;
          proposals.push_back(obj);
        }
      }
    }
  }

  // sort all proposals by score from highest to lowest
  qsort_descent_inplace(proposals);
  // apply nms with nms_threshold
  const float nms_threshold = 0.45f;
  std::vector<int> picked;
  nms_sorted_bboxes(proposals, picked, nms_threshold);
  int count = picked.size();

  objects.resize(count);
  for (int i = 0; i < count; i++) {
    objects[i] = proposals[picked[i]];

    // adjust offset to original unpadded
    float x0 = (objects[i].rect.x - (wpad / 2)) / scale;
    float y0 = (objects[i].rect.y - (wpad / 2)) / scale;
    float x1 = (objects[i].rect.x + objects[i].rect.width - (wpad / 2)) / scale;
    float y1 =
        (objects[i].rect.y + objects[i].rect.height - (wpad / 2)) / scale;

    // clip
    x0 = std::max(std::min(x0, (float)(img_w - 1)), 0.f);
    y0 = std::max(std::min(y0, (float)(img_h - 1)), 0.f);
    x1 = std::max(std::min(x1, (float)(img_w - 1)), 0.f);
    y1 = std::max(std::min(y1, (float)(img_h - 1)), 0.f);

    objects[i].rect.x = x0;
    objects[i].rect.y = y0;
    objects[i].rect.width = x1 - x0;
    objects[i].rect.height = y1 - y0;

    // printf("[%d] %d,%d,%d,%d\n", i, objects[i].rect.x, objects[i].rect.y, objects[i].rect.width, objects[i].rect.height);
  }

  // 增加输出
  if (pOut) {
    pOut->count = objects.size();
    for (size_t i = 0; i < objects.size(); i++) {
      if (i >= OBJDETECTMAX) {
        pOut->count = OBJDETECTMAX;
        break;
      }
      /* code */
      pOut->objs[i].x = (int)objects[i].rect.x;
      pOut->objs[i].y = (int)objects[i].rect.y;
      pOut->objs[i].w = (int)objects[i].rect.width;
      pOut->objs[i].h = (int)objects[i].rect.height;
      pOut->objs[i].label = objects[i].label;
      pOut->objs[i].score = objects[i].prob;
    }
  }
  draw_objects(objects);
  return 0;
}