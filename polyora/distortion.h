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
/* Camera Distortion handling functions. Julien Pilet 2008. */

#ifndef _DISTORTION_H
#define _DISTORTION_H

#include "keypoint.h"

// Camera calibration;
struct DistortParams {
	float fx,fy,cx,cy;
	float k[2],p[2];

	static DistortParams *load(const char *file);
};

extern DistortParams defaultCam;

/*! Computes distorted coordinates from normalized ones. */
void distort(const float u, const float v, float distorted[2], const DistortParams &p=defaultCam);

/*! Undistort image coordinates to normalized ones. */
void undistort(const float distorted_u, const float distorted_v, float uv[2], const DistortParams &p=defaultCam);

/*! convenient function to undistort a keypoint */
void undistort(keypoint *kpt, const DistortParams &p=defaultCam);

#endif
