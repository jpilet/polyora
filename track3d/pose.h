#ifndef POSE_H
#define POSE_H

#include <vector>

#include "camera.h"
#include <polyora/vobj_tracker.h>

void getCorrespondencesForInstance(
    const vobj_frame *frame, const vobj_instance *instance,
    std::vector<cv::Point3f> *object_points,
    std::vector<cv::Point2f> *projections);

void computeObjectPose(const vobj_frame *frame, const vobj_instance *instance,
                       PerspectiveCamera *camera);

#endif  // POSE_H
