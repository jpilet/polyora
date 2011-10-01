#ifndef MYPYRLK_H
#define MYPYRLK_H

#include "pyrimage.h"
void myCalcOpticalFlowPyrLK( const PyrImage* arrA, const PyrImage* arrB,
                        const CvPoint2D32f * featuresA,
                        CvPoint2D32f * featuresB,
                        int count, CvSize winSize, int level,
                        char *status, float *error,
                        CvTermCriteria criteria, int flags );

#endif
