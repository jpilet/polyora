#ifndef POSE_H
#define POSE_H

#include <vector>

#include "camera.h"
#include <polyora/vobj_tracker.h>

void computeObjectPose(const vobj_frame *frame, const vobj_instance *instance,
                       PerspectiveCamera *camera);

#endif  // POSE_H
