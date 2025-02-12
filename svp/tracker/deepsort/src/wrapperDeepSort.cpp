#include "wrapperDeepSort.h"
#include "tracker.h"
#include <iostream>


#include "detectobjs.h" // ho+

const int nn_budget = 100;
const float max_cosine_distance = 0.2;

// 全局变量，用于存储C++的跟踪对象
tracker *deeptracker = nullptr;

// 初始化跟踪器
int deepsort_init() {
  if (deeptracker != nullptr) {
    delete deeptracker; // 确保不会重复创建
  }
  deeptracker = new tracker(max_cosine_distance, nn_budget);
  return 0; // 返回0表示成功
}

void get_detections(DETECTBOX box, float confidence, DETECTIONS &d) {
  DETECTION_ROW tmpRow;
  tmpRow.tlwh = box; // DETECTBOX(x, y, w, h);

  tmpRow.confidence = confidence;
  d.push_back(tmpRow);
}

void track_deepsort(stYolovDetectObjs *pOut) {
  DETECTIONS detections;
  for (int i = 0; i < pOut->count; i++) {
    DETECTBOX box(pOut->objs[i].x, pOut->objs[i].y, pOut->objs[i].w,
                  pOut->objs[i].h);
    DETECTION_ROW tmpRow;
    tmpRow.tlwh = box;
    tmpRow.confidence = pOut->objs[i].score;
    for (int j = 0; j < FEATURE_DIM; j++) {
      tmpRow.feature[j] = pOut->objs[i].feature[j];
    }
    detections.push_back(tmpRow);
  }
  deeptracker->predict();          // 调用跟踪器的预测方法。
  deeptracker->update(detections); // 使用新的检测结果更新跟踪器
  // ### 处理跟踪结果:
  std::vector<RESULT_DATA> result; // 初始化一个 result 向量来存储跟踪结果
  for (Track &track : deeptracker->tracks) {
    if (!track.is_confirmed() || track.time_since_update > 1)
      continue; // 如果跟踪对象未确认或更新时间超过1帧，则跳过。
    result.push_back(std::make_pair(track.track_id, track.to_tlwh()));
    // 将确认的跟踪对象的ID和位置（to_tlwh()）存储到
    // result 向量中。
  }
  for (const auto &res : result) {
    std::cout << "TrackID: " << res.first << ", "
              << "Bounding Box (x, y, w, h): (" << res.second[0] << ", "
              << res.second[1] << ", " << res.second[2] << ", " << res.second[3]
              << ")" << std::endl;
  }
  pOut->id_count = result.size();
  for (int i = 0; i < result.size(); i++) {
    pOut->id_objs[i].id = result[i].first;
    pOut->id_objs[i].x = result[i].second[0];
    pOut->id_objs[i].y = result[i].second[1];
    pOut->id_objs[i].w = result[i].second[2];
    pOut->id_objs[i].h = result[i].second[3];
  }
}
