#include "pose.h"

namespace {

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

}  // namespace

void computeObjectPose(const vobj_frame *frame, const vobj_instance *instance,
                       PerspectiveCamera *camera) {
  std::vector<cv::Point3f> object_points;
  std::vector<cv::Point2f> projections;

  getCorrespondencesForInstance(frame, instance, &object_points, &projections);

  camera->setPoseFromHomography(instance->transform);

  printf("Computing pose from %d matches.\n", projections.size());

  if (projections.size() > 4) {
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
}

PoseFilter::PoseFilter(float alpha) : alpha_(alpha) {
	assert(alpha > 0.0f);
	assert(alpha <= 1.0f);
}

void PoseFilter::update(PerspectiveCamera *camera) {
	cv::addWeighted(rotation_, 1.0 - alpha_, camera->getExpMapRotation(), alpha_, 0, rotation_);
	camera->setExpMapRotation(rotation_);

	cv::addWeighted(translation_, 1.0 - alpha_, camera->getTranslation(), alpha_, 0, translation_);
	camera->setTranslation(translation_);
}

void PoseFilter::reset(const PerspectiveCamera *camera) {
	rotation_ = camera->getExpMapRotation();
	translation_ = camera->getTranslation();
}

KalmanPoseFilter::KalmanPoseFilter() : resetOnNextUpdate_(true) { }

void KalmanPoseFilter::reset(PerspectiveCamera *camera) {
    resetOnNextUpdate_ = false;

    // The filter has 3 DOF for the position, 3 DoF for the rotation.
    // The state is made of x + dx, => 12 DoF.
    kalman_.init(12, 6);

    kalman_.transitionMatrix = *(cv::Mat_<float>(12, 12)
         << 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, // x1 = x0 + dx0
            0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0,
            0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0,
            0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0,
            0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1,
            0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, // dx1 = dx0
            0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1);

    // The measurements correspond to the 6 first state values.
    cv::setIdentity(kalman_.measurementMatrix);

    cv::setIdentity(kalman_.processNoiseCov, cv::Scalar::all(1.0));
    cv::setIdentity(kalman_.processNoiseCov(cv::Range(0,3), cv::Range(0,3)),
                    cv::Scalar::all(1e-7));

    cv::setIdentity(kalman_.measurementNoiseCov, cv::Scalar::all(10));
    cv::setIdentity(kalman_.measurementNoiseCov(cv::Range(0,3), cv::Range(0,3)),
                    cv::Scalar::all(1));

    cv::setIdentity(kalman_.errorCovPost, cv::Scalar::all(1));

    kalman_.statePost.rowRange(0, 6) = getMeasurement(camera);
    kalman_.statePost.rowRange(7, 12) = cv::Mat::zeros(1, 6, CV_32F);
}

cv::Mat KalmanPoseFilter::getMeasurement(PerspectiveCamera *camera) {
    cv::Mat measurement(6, 1, CV_32FC1);

    cv::Mat rotation(camera->getExpMapRotation());
    rotation.convertTo(measurement.rowRange(0, 3), CV_32FC1);

    cv::Mat translation(camera->getTranslation());
    translation.convertTo(measurement.rowRange(3, 6), CV_32FC1);
    return measurement;
}

void KalmanPoseFilter::setCamera(PerspectiveCamera *camera) {
    cv::Mat rotation;
    kalman_.statePre.rowRange(0, 3).convertTo(rotation, CV_32FC1);
    camera->setExpMapRotation(rotation);

    cv::Mat translation;
    kalman_.statePre.rowRange(3, 6).convertTo(translation, CV_32FC1);
    camera->setTranslation(translation);
}

void KalmanPoseFilter::update(PerspectiveCamera *camera) {
    if (resetOnNextUpdate_) {
        resetOnNextUpdate_ = false;
        reset(camera);
    } else {
        kalman_.predict();
        kalman_.correct(getMeasurement(camera));
        setCamera(camera);
    }
}
