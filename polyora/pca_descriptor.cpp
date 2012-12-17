#include "pca_descriptor.h"

#include "kpttracker.h"
#include <assert.h>
#include <math.h>
#include <algorithm>

#ifndef M_2PI
#define M_2PI 6.283185307179586476925286766559f
#endif

using cv::Mat;
using std::max;
using std::min;

namespace {

// Normalizes a patch for illumination.
void IlluminationNormalizeMeanStdDev(const Mat& warped, Mat* dest) {
    cv::Scalar mean, stddev;
    //cv::Range range(warped.rows / 4, (warped.rows * 3) / 4);
    cv::meanStdDev(warped, mean, stddev);

    double normalize = (fabs(stddev[0]) < 1 ? 1.0 / 255.0 : 1.0 / stddev[0]);
    warped.convertTo(*dest, dest->type(), normalize, - mean[0] * normalize);
}

void IlluminationNormalizeEqualizeHistogram(const Mat& warped, Mat* dest) {
    Mat equalized(warped.size(), CV_8UC1);
    cv::equalizeHist(warped, equalized);
    equalized.convertTo(*dest, dest->type(), 1.0/255.0, 0);
}

void IlluminationNormalize(const Mat& warped, Mat* dest) {
    if (1) {
	IlluminationNormalizeEqualizeHistogram(warped, dest);
    } else {
	IlluminationNormalizeMeanStdDev(warped, dest);
    }
}

}  // namespace

void ExtractPatch(const pyr_keypoint& point, cv::Size patch_size, Mat* dest) {
    assert(dest != 0);
    assert(point.descriptor.orientation >= 0);
    assert(point.descriptor.orientation <= M_2PI);

    pyr_frame *frame = static_cast<pyr_frame *>(point.frame);
    int scale = max(0, min(frame->pyr->nbLev - 1, static_cast<int>(point.scale)));
    IplImage *im = frame->pyr->images[scale];

    /*
       To convert patch coordinates to image coordinates,
       the following operations are applied:
       1) Translate(-patch_width / 2, - patch_height /2);
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
    Mat transform(2, 3, CV_64FC1, transform_data);
    Mat warped(patch_size, CV_8UC1);
    cv::warpAffine(Mat(im), warped, transform, patch_size,
	    cv::INTER_LINEAR + cv::WARP_INVERSE_MAP, cv::BORDER_REPLICATE);

    // Normalize and convert to float
    IlluminationNormalize(warped, dest);
}


cv::Mat LoadPcaVectors(int num_vectors) {
    int size = patch_tagger::patch_size * patch_tagger::patch_size;
    assert(num_vectors > 0 && num_vectors < size);
    cv::Mat data(size, size, CV_32FC1);
    FILE *f = fopen("pca_modes", "r");
    if (!f) {
        perror("pca_modes");
        exit(-1);
    }
    for (int i = 0; i < data.rows * data.cols; ++i) {
        if (fscanf(f, "%f", data.ptr<float>() + i) != 1) {
            fprintf(stderr, "pca_modes: can't read entry %d\n", i);
        }
    }
    fclose(f);
    return data(cv::Range::all(), cv::Range(0, num_vectors));
}

void PcaProject(const cv::Mat& pca_modes, const cv::Mat& vector, cv::Mat* dest) {
    *dest = pca_modes * vector;
}

