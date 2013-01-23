#ifndef POSE_H
#define POSE_H

#include <vector>
#include <opencv2/core/core.hpp>

#include "camera.h"
#include <polyora/vobj_tracker.h>

void computeObjectPose(const vobj_frame *frame, const vobj_instance *instance,
                       PerspectiveCamera *camera);

class KalmanPoseFilter {
  public:
    KalmanPoseFilter();

    void update(PerspectiveCamera *camera);

    void reset(PerspectiveCamera *camera);

  private:
    cv::Mat getMeasurement(PerspectiveCamera *camera);
    void setCamera(PerspectiveCamera *camera);

    bool resetOnNextUpdate_;
    cv::KalmanFilter kalman_;
};

#endif  // POSE_H
