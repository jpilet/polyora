#ifndef _PCADESCRIPTOR_H
#define _PCADESCRIPTOR_H

#include <opencv2/core/core.hpp>

struct pyr_keypoint;

void ExtractPatch(const pyr_keypoint& point,
	          cv::Size patch_size, cv::Mat* dest);

class PatchPCA {
  public:
    bool load(int num_vectors);

    void project(const cv::Mat& vector, cv::Mat* dest) const;
    void unproject(const cv::Mat& compressed, cv::Mat* dst) const;

  private:
    cv::Mat pca_modes_;
    cv::Mat pca_average_;
    int num_vectors_;
};

#endif  // _PCADESCRIPTOR_H
