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
#ifndef HOMOGRAPHY4_H
#define HOMOGRAPHY4_H

#include "fvec4.h"

void homography4_from_4pt(const fvec4 x[2], const fvec4 y[2], const fvec4 z[2], const fvec4 w[2], fvec4 cgret[9]);
void homography4_transform(const fvec4 a[2], const fvec4 H[3][3], fvec4 r[2]);
void homography4_from_4corresp(
		const fvec4 a[2], const fvec4 b[2], const fvec4 c[2], const fvec4 d[2],
		const fvec4 x[2], const fvec4 y[2], const fvec4 z[2], const fvec4 w[2], fvec4 R[3][3]);

/*! High-speed SSE optimised RANSAC to find homographies.

  uv1 points to a matrix of coordinates. Coordinate u and v are packed. Adding
  'stride1' bytes to uv1 points to the next point. uv2 and stride2 follow the same logic.
  n is the number of points.
  maxiter is the maximum number of iterations
  dist_threshold is a reprojection distrance threshold used to differenciate inliers from outliers
  inliers_mask is either 0 or a pointer to an array of n chars set to 0xff for inliers and to 0 for outliers.
  inliers1 and inliers2 are optional pointers to array that will be filled with inlier correspondences.

  the function return the number of correspondances supporting the returned homography.
*/

int ransac_h4(const float *uv1, int stride1, const float *uv2, int stride2, int n, 
		int maxiter, float dist_threshold, int stop_support, 
		float result[3][3], char *inliers_mask, float *inliers1, float *inliers2);
#endif
