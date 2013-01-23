#ifndef POSE_H
#define POSE_H

#include <vector>
#include <opencv2/core/core.hpp>

#include "camera.h"
#include <polyora/vobj_tracker.h>

void computeObjectPose(const vobj_frame *frame, const vobj_instance *instance,
                       PerspectiveCamera *camera);

//! Filter a camera pose.
//! When polyora tracks an object in 3D, the result might be noisy.
//! This class filters out the noise, at the cost of adding a small delay.
class PoseFilter {
public:
	PoseFilter(float alpha = defaultAlpha());

	//! Alpha should be in the range ]0, 1]. A value of 1.0 means: no filtering
	//! at all, a smaller value adds more delay and reduces noise further. 
	void setAlpha(float alpha) { alpha_ = alpha; }

	static float defaultAlpha() { return 0.4f; }

	//! Reset should be called for frames in which the object appears.
	void reset(const PerspectiveCamera *camera);

	//! Update should be called for frames following a reset frame,
	//! or an update frame.
	void update(PerspectiveCamera *camera);

private:
	float alpha_;
	cv::Mat rotation_;
	cv::Mat translation_;
};

//! The KalmanPoseFilter works like PoseFilter, but using Kalman's approach.
//! The filter is not tuned yet.
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
