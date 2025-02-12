#ifndef MODEL_H
#define MODEL_H
#include "dataType.h"

// * Each rect's data structure.
// * tlwh: topleft point & (w,h)
// * confidence: detection confidence.
// * feature: the rect's 128d feature.
// */
class DETECTION_ROW {
public:
  DETECTBOX tlwh; // 1行4列的矩阵
  float confidence;
  FEATURE feature;
  DETECTBOX to_xyah() const;
  DETECTBOX to_tlbr() const;
};

typedef std::vector<DETECTION_ROW> DETECTIONS;

#endif // MODEL_H
