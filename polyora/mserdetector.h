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
#ifndef MSERDETECTOR_H
#define MSERDETECTOR_H

#include <opencv/cv.h>

struct MSERegion
{
	cv::Point_<float> c,e1,e2;
};

class MSERDetector
{

public:
	MSERDetector();
	unsigned mser(cv::Mat &mat, cv::Mat mask);

	CvMSERParams params;
	cv::MemStorage storage; 
	cv::Seq<CvSeq*> contours;

	typedef cv::Seq<cv::Point> Contour;
	typedef cv::SeqIterator<CvSeq*> iterator;

	iterator begin() { return contours.begin(); }
	unsigned size() { return contours.size(); }

	void getRegion(Contour &c, MSERegion &r);
	MSERegion getRegion(Contour &c) {
		MSERegion r;
		getRegion(c,r);
		return r;
	}

private:
	cv::PCA pca;
	cv::Mat_<float> mean;
};
#endif
