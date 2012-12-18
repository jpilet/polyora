#include "pose.h"

void getCorrespondencesForInstance(
    const vobj_frame *frame_, const vobj_instance *instance,
    std::vector<cv::Point3f> *object_points,
    std::vector<cv::Point2f> *projections) {
  object_points->clear();
  projections->clear();

  // vobj_frame does not have const_iterator yet. We have to const_cast :(
  vobj_frame *frame = const_cast<vobj_frame *>(frame_);
  for (tracks::keypoint_frame_iterator it(frame->points.begin());
       !it.end(); ++it) {
    const vobj_keypoint *kpt = static_cast<vobj_keypoint *>(it.elem());
    if (kpt->vobj != instance->object || kpt->obj_kpt == 0) {
      continue;
    }

    object_points->push_back(cv::Point3f(
            kpt->obj_kpt->u,
            kpt->obj_kpt->v,
            0));
    projections->push_back(cv::Point2f(
            kpt->u,
            kpt->v));
  }
}

void computeObjectPose(const vobj_frame *frame, const vobj_instance *instance,
                       PerspectiveCamera *camera) {
  std::vector<cv::Point3f> object_points;
  std::vector<cv::Point2f> projections;

  getCorrespondencesForInstance(frame, instance, &object_points, &projections);

  camera->setPoseFromHomography(instance->transform);
  cv::Mat intrinsics = camera->getIntrinsics();
  cv::Mat rotation = camera->getExpMapRotation();
  cv::Mat translation = camera->getTranslation();
  cv::solvePnP(object_points, projections, intrinsics, camera->distortion,
               rotation, translation, 
               true, // use extrinsic guess
               CV_ITERATIVE);
  camera->setExpMapRotation(rotation);
  camera->setTranslation(translation);

}

