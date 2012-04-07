#include "pca_descriptor.h"

#include "kpttracker.h"
#include <assert.h>
#include <math.h>
#include <algorithm>

#ifndef M_2PI
#define M_2PI 6.283185307179586476925286766559f
#endif

using std::max;
using std::min;

namespace {

// Normalizes a patch for illumination.
void IlluminationNormalize(const cv::Mat& warped, cv::Mat* dest) {
    cv::Scalar mean, stddev;
    //cv::Range range(warped.rows / 4, (warped.rows * 3) / 4);
    cv::meanStdDev(warped, mean, stddev);

    double normalize = (stddev[0] == 0 ? 1.0 / 255.0 : 1.0 / stddev[0]);
    warped.convertTo(*dest, dest->type(), normalize, - mean[0] * normalize);
}

}  // namespace

void ExtractPatch(const pyr_keypoint& point, cv::Size patch_size, cv::Mat* dest) {
    assert(dest != 0);
    assert(point.descriptor.orientation >= 0);
    assert(point.descriptor.orientation <= M_2PI);

    pyr_frame *frame = static_cast<pyr_frame *>(point.frame);
    int scale = max(0, min(frame->pyr->nbLev - 1, static_cast<int>(point.scale)));
    IplImage *im = frame->pyr->images[scale];

    /*
       To convert patch coordinates to image coordinates,
       the following operations are applied:
       1) Translate(-patch_width / 2, - path_height /2);
       2) Rotate(orientation)
       3) Scale(pow(2, scale) - 1 << floor(scale)) (ignored for now)
       4) Translate(level.u, level.v)
     */

    const double tx = - patch_size.width / 2.0f;
    const double ty = - patch_size.height / 2.0f;
    const double t2x = point.level.u;
    const double t2y = point.level.v;
    const double ca = cos(point.descriptor.orientation);
    const double sa = sin(point.descriptor.orientation);

    double transform_data[6] = {
	ca, -sa, ca*tx - sa*ty + t2x,
	sa, ca, sa*tx + ca*ty + t2y,
    };
    cv::Mat transform(2, 3, CV_64FC1, transform_data);
    cv::Mat warped;
    cv::warpAffine(cv::Mat(im), warped, transform, patch_size,
	    cv::INTER_LINEAR + cv::WARP_INVERSE_MAP, cv::BORDER_REPLICATE);

    // Normalize and convert to float
    IlluminationNormalize(warped, dest);
}
