#ifndef _PCADESCRIPTOR_H
#define _PCADESCRIPTOR_H

#include <opencv/cv.h>

struct pyr_keypoint;

void ExtractPatch(const pyr_keypoint& point,
	          cv::Size patch_size, cv::Mat* dest);

#endif  // _PCADESCRIPTOR_H
