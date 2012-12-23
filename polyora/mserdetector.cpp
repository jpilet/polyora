/*  This file is part of Polyora, a multi-target tracking library.
    Copyright (C) 2010 Julien Pilet

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program. If not, see <http://www.gnu.org/licenses/>.

    To contact the author of this program, please send an e-mail to:
    julien.pilet(at)calodox.org
*/
#include "mserdetector.h"

MSERDetector::MSERDetector() : storage(cvCreateMemStorage(0)), mean(cv::Size(2,1))
{
	params = cvMSERParams();
	params.minArea=3;
	params.delta=5;
	params.minDiversity=0.7f;
	params.maxVariation = .15f;
	params.maxArea=300;
}

unsigned MSERDetector::mser(cv::Mat &mat, cv::Mat mask)
{
	CvMat _image = mat, _mask, *maskp=0;
	if (mask.data) {
		_mask = mask;
		maskp = &_mask;
	}

	// mark previously allocated memory as available.
	// invalidates previous contours.
	cvClearMemStorage(storage);
	cvExtractMSER(&_image, maskp, &contours.seq, storage, params );

	return size();
}

void MSERDetector::getRegion(Contour &contour, MSERegion &r)
{
	int n =contour.size();
	cv::Mat_<float> dat(cv::Size(2,n));
	float *ptr= &dat(0,0);
	mean(0,0)=mean(0,1)=0;
	for (cv::Seq<cv::Point>::iterator j(contour.begin()); --n>=0; ++j) {
		mean(0,0) += ptr[0] = (*j).x;
		mean(0,1) += ptr[1] = (*j).y;
		ptr+=2;
	}
	mean *= 1.0/contour.size();
	r.c.x = mean.at<float>(0,0), r.c.y = mean.at<float>(0,1);

	pca(dat, mean, CV_PCA_DATA_AS_ROW, 2);

	float a = 1.5f*sqrtf(pca.eigenvalues.at<float>(0,0));
	float b = 1.5f*sqrtf(pca.eigenvalues.at<float>(0,1));
	cv::Point_<float> e1(a*pca.eigenvectors.at<float>(0,0), a*pca.eigenvectors.at<float>(0,1));
	cv::Point_<float> e2(b*pca.eigenvectors.at<float>(1,0), b*pca.eigenvectors.at<float>(1,1));

	if (e1.x*e2.y-e1.y*e2.x<0) {
		e2 = -e2;
	}
	r.e1 = e1;
	r.e2 = e2;
}


