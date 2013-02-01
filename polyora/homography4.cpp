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
#include "fvec4.h"

#include <assert.h>
//#define DEBUG_CMPHOMO
#include <iostream>



std::ostream &operator<<(std::ostream &s, const fvec4& a) {
	s << "[";
	for (int i=0; i<fvec4::size; i++) {
		s.width(12);
		s.precision(5);
		s << a[i];
	}
	s << "]";
	return s;
}


/*! computes the 4 homographies sending [0,0] , [0,1], [1,1] and [1,0]
 * to x,y,z and w.
 */
void homography4_from_4pt(const fvec4 x[2], const fvec4 y[2], const fvec4 z[2], const fvec4 w[2], fvec4 cgret[9])
{
	fvec4 t1 = x[0];
	fvec4 t2 = z[0];
	fvec4 t4 = y[1];
	fvec4 t5 = t1 * t2 * t4;
	fvec4 t6 = w[1];
	fvec4 t7 = t1 * t6;
	fvec4 t8 = t2 * t7;
	fvec4 t9 = z[1];
	fvec4 t10 = t1 * t9;
	fvec4 t11 = y[0];
	fvec4 t14 = x[1];
	fvec4 t15 = w[0];
	fvec4 t16 = t14 * t15;
	fvec4 t18 = t16 * t11;
	fvec4 t20 = t15 * t11 * t9;
	fvec4 t21 = t15 * t4;
	fvec4 t24 = t15 * t9;
	fvec4 t25 = t2 * t4;
	fvec4 t26 = t6 * t2;
	fvec4 t27 = t6 * t11;
	fvec4 t28 = t9 * t11;
	fvec4 t30 =  fvec4(1)/(t21 -t24 - t25 + t26 - t27 + t28);
	fvec4 t32 = t1 * t15;
	fvec4 t35 = t14 * t11;
	fvec4 t41 = t4 * t1;
	fvec4 t42 = t6 * t41;
	fvec4 t43 = t14 * t2;
	fvec4 t46 = t16 * t9;
	fvec4 t48 = t14 * t9 * t11;
	fvec4 t51 = t4 * t6 * t2;
	fvec4 t55 = t6 * t14;
	cgret[0] = -(t8 -t5 + t10 * t11 - t11 * t7 - t16 * t2 + t18 - t20 + t21 * t2) * t30;
	cgret[1] = (t5 - t8 - t32 * t4 + t32 * t9 + t18 - t2 * t35 + t27 * t2 - t20) * t30;
	cgret[2] = t1;
	cgret[3] = (-t9 * t7 + t42 + t43 * t4 - t16 * t4 + t46 - t48 + t27 * t9 - t51) * t30;
	cgret[4] = (-t42 + t41 * t9 - t55 * t2 + t46 - t48 + t55 * t11 + t51 - t21 * t9) * t30;
	cgret[5] = t14;
	cgret[6] = (-t10 + t41 + t43 - t35 + t24 - t21 - t26 + t27) * t30;
	cgret[7] = (-t7 + t10 + t16 - t43 + t27 - t28 - t21 + t25) * t30;
        cgret[8] = fvec4(1);
	
}

inline void homography_transform(const float a[2], const float H[3][3], float r[2])
{
	float z = 1.0f/(H[2][0]*a[0] + H[2][1]*a[1] + H[2][2]);
	r[0] = (H[0][0]*a[0] + H[0][1]*a[1] + H[0][2])*z;
	r[1] = (H[1][0]*a[0] + H[1][1]*a[1] + H[1][2])*z;
}


inline void homography4_transform(const fvec4 a[2], const fvec4 H[3][3], fvec4 r[2])
{
	fvec4 z = fvec4(1)/(H[2][0]*a[0] + H[2][1]*a[1] + H[2][2]);
	r[0] = (H[0][0]*a[0] + H[0][1]*a[1] + H[0][2])*z;
	r[1] = (H[1][0]*a[0] + H[1][1]*a[1] + H[1][2])*z;
}

#ifdef DEBUG_CMPHOMO
#include <iostream>

static bool eps_cmp2(const fvec4 a[2], const fvec4 b[2])
{
	float eps = 1e-1;
	fvec4 dx = a[0]-b[0], dy =a[1]-b[1];

	fvec4 dx2 = dx*dx;
	fvec4 dy2 = dy*dy;
	for (int i=0; i<fvec4::size; i++) {
		if (finite(dx2[i]) && finite(dy2[i]) &&
				dx2[i] > eps || dy2[i]>eps) {
			std::cout << "(" << a[0][i] << "," << a[1][i] << ") should be at (" << b[0][i] << "," << b[1][i] << ")\n";
			return false;
		}
	}
	return true;
	//return (dx*dx <eps && dy*dy<eps);
}
#endif

void homography4_from_4corresp(
		const fvec4 a[2], const fvec4 b[2], const fvec4 c[2], const fvec4 d[2],
		const fvec4 x[2], const fvec4 y[2], const fvec4 z[2], const fvec4 w[2], fvec4 R[3][3])
{
	fvec4 Hr[3][3], Hl[3][3];

	homography4_from_4pt(a,b,c,d,&Hr[0][0]);
	homography4_from_4pt(x,y,z,w,&Hl[0][0]);

	// the following code computes R = Hl * inverse Hr
	fvec4 t2 = Hr[1][1]-Hr[2][1]*Hr[1][2];
	fvec4 t4 = Hr[0][0]*Hr[1][1];
	fvec4 t5 = Hr[0][0]*Hr[1][2];
	fvec4 t7 = Hr[1][0]*Hr[0][1];
	fvec4 t8 = Hr[0][2]*Hr[1][0];
	fvec4 t10 = Hr[0][1]*Hr[2][0];
	fvec4 t12 = Hr[0][2]*Hr[2][0];
	fvec4 t15 = fvec4(1)/(t4-t5*Hr[2][1]-t7+t8*Hr[2][1]+t10*Hr[1][2]-t12*Hr[1][1]);
	fvec4 t18 = -Hr[1][0]+Hr[1][2]*Hr[2][0];
	fvec4 t23 = -Hr[1][0]*Hr[2][1]+Hr[1][1]*Hr[2][0];
	fvec4 t28 = -Hr[0][1]+Hr[0][2]*Hr[2][1];
	fvec4 t31 = Hr[0][0]-t12;
	fvec4 t35 = Hr[0][0]*Hr[2][1]-t10;
	fvec4 t41 = -Hr[0][1]*Hr[1][2]+Hr[0][2]*Hr[1][1];
	fvec4 t44 = t5-t8;
	fvec4 t47 = t4-t7;
	fvec4 t48 = t2*t15;
	fvec4 t49 = t28*t15;
	fvec4 t50 = t41*t15;
	R[0][0] = Hl[0][0]*t48+Hl[0][1]*(t18*t15)-Hl[0][2]*(t23*t15);
	R[0][1] = Hl[0][0]*t49+Hl[0][1]*(t31*t15)-Hl[0][2]*(t35*t15);
	R[0][2] = -Hl[0][0]*t50-Hl[0][1]*(t44*t15)+Hl[0][2]*(t47*t15);
	R[1][0] = Hl[1][0]*t48+Hl[1][1]*(t18*t15)-Hl[1][2]*(t23*t15);
	R[1][1] = Hl[1][0]*t49+Hl[1][1]*(t31*t15)-Hl[1][2]*(t35*t15);
	R[1][2] = -Hl[1][0]*t50-Hl[1][1]*(t44*t15)+Hl[1][2]*(t47*t15);
	R[2][0] = Hl[2][0]*t48+Hl[2][1]*(t18*t15)-t23*t15;
	R[2][1] = Hl[2][0]*t49+Hl[2][1]*(t31*t15)-t35*t15;
	R[2][2] = -Hl[2][0]*t50-Hl[2][1]*(t44*t15)+t47*t15;

#ifdef DEBUG_CMPHOMO
	// sanity check
	fvec4 uv[2];
	homography4_transform(a, R, uv);
	assert(eps_cmp2(uv,x));

	homography4_transform(b, R, uv);
	assert(eps_cmp2(uv,y));

	homography4_transform(c, R, uv);
	assert(eps_cmp2(uv,z));

	homography4_transform(d, R, uv);
	assert(eps_cmp2(uv,w));
#endif
}

static inline float rand_range(unsigned long n) {
	// not smart at all.
	int rnd = rand();
	float r = (float)floor(double(n)*double(rnd)/(double(RAND_MAX)+1.0));
	return r;
}

static inline fvec4 rand_range4(int n) {
	return fvec4( rand_range(n), rand_range(n), rand_range(n), rand_range(n) );
}

static inline const float *row(int row, const float *array, int stride)
{
	return (const float *)(((char*)array)+row*stride);
}

static inline fvec4 dist2(const fvec4 a[2], const fvec4 b[2]) { fvec4 dx(a[0]-b[0]); fvec4 dy(a[1]-b[1]); return dx*dx + dy*dy; }
static inline float dist2(const float a[2], const float b[2]) { float dx(a[0]-b[0]); float dy(a[1]-b[1]); return dx*dx + dy*dy; }

int ransac_h4(const float *uv1, int stride1, const float *uv2, int stride2, int n, 
		int maxiter, float dist_threshold, int stop_support, 
		float result[3][3], char *inliers_mask, float *inliers1, float *inliers2)
{
	fvec4 bestH[3][3];
	fvec4 best_support(0);
	fvec4 threshold(dist_threshold*dist_threshold);

	if (n<5) return 0;

	for (int iter=0; iter<maxiter; ++iter) {
		// draw 4 random correspondences
		fvec4 corresp[4];
		//std::cout << "Raw random:\n";
		for (int i=0;i<4;i++) {
			corresp[i] = rand_range4(std::min(6  + iter, n) -i);
			//std::cout << corresp[i] << "\n";
			for (int j=0; j<i; j++) {
				corresp[i] += ((corresp[j] <= corresp[i]) & fvec4(1));
			}
			// hack to sort the array
			if (i==1) {
				fvec4 cmin = min(corresp[0],corresp[1]);
				fvec4 cmax = max(corresp[0],corresp[1]);
				corresp[0] = cmin;
				corresp[1] = cmax;
			}
			if (i==2) {
				fvec4 min01 = corresp[0]; // already sorted
				fvec4 max01 = corresp[1];
				fvec4 min12 = min(corresp[1],corresp[2]);
				fvec4 min02 = min(corresp[0],corresp[2]);
				corresp[0] = min(min01,corresp[2]);
				corresp[1] = max(max(min01,min12), min02);
				corresp[2]= max(max01,corresp[2]);
			}
		}
		
		if (0) {
			std::cout << "Iteration: " << iter << ", correspondences after sorting:\n";
			for (int i=0;i<4;i++) {
				std::cout << corresp[i] << "\n";
			}
		}
		

		// fetch the 4 corresp
		fvec4 pts1[4][2];
		fvec4 pts2[4][2];
		for (int i=0; i<4; i++) {
			for (int j=0; j<fvec4::size; j++) {
				int r = (int)floorf(corresp[i][j]);
				assert (r>=0);
				assert (r<n);
				pts1[i][0][j] = row(r, uv1, stride1)[0];
				pts1[i][1][j] = row(r, uv1, stride1)[1];
				pts2[i][0][j] = row(r, uv2, stride2)[0];
				pts2[i][1][j] = row(r, uv2, stride2)[1];
			}
		}

		// compute the homography
		fvec4 H[3][3];
		homography4_from_4corresp(
				pts1[0], pts1[1], pts1[2], pts1[3],
				pts2[0], pts2[1], pts2[2], pts2[3],
				H);

		// evaluate support
		fvec4 support(0);
		for (int i=0; i<n; i++) {
			fvec4 p[2], g[2];
			p[0] = fvec4( row(i,uv1,stride1)[0] );
			p[1] = fvec4( row(i,uv1,stride1)[1] );
			g[0] = fvec4( row(i,uv2,stride2)[0] );
			g[1] = fvec4( row(i,uv2,stride2)[1] );
			fvec4 t[2];
			homography4_transform(p, H, t);
			fvec4 d = dist2(t,g);
			support += (d < threshold) & fvec4(1 - .1 * (d / threshold));
		}

		// remember the best solution
		fvec4 replace = support > best_support;
		fvec4 keep = replace ^ (fvec4(0)<fvec4(1));
		best_support = (support & replace)  | (best_support & keep);

		/*
		std::cout << "Support : " << support << " ";
		std::cout << "Best Sup: " << best_support << std::endl;

		for (int i=0; i<3; i++)
			for (int j=0;j<3; j++)
				std::cout << bestH[i][j] << " -> " <<  ((H[i][j] & replace) | (bestH[i][j] & keep))<< std::endl;
		*/

		for (int i=0; i<3; i++)
			for (int j=0;j<3; j++)
				bestH[i][j] = (H[i][j] & replace) | (bestH[i][j] & keep);


		// early termination ?
		if (best_support.horizontal_max() >= stop_support) break;
	}


	int s = best_support.horizontal_max_index();
	for (int i=0; i<3; i++)
		for (int j=0;j<3; j++)
			result[i][j] = bestH[i][j][s];

	int final_support = 0;

		float t2 = threshold[0];
		// TODO: use SIMD
		float *in1 = inliers1;
		float *in2 = inliers2;
		for (int i=0; i<n; i++) {
			float t[2];
			homography_transform(row(i,uv1,stride1), result, t);
			bool inlier = dist2(t, row(i,uv2,stride2)) < t2;

			if (inlier) {
				++final_support;
			}

			if (inliers_mask) inliers_mask[i] = ( inlier ? 0xFF : 0);
			if (inlier) {
				if (inliers1) {
					*in1++ = row(i,uv1,stride1)[0];
					*in1++ = row(i,uv1,stride1)[1];
				}
				if (inliers2) {
					*in2++ = row(i,uv2,stride2)[0];
					*in2++ = row(i,uv2,stride2)[1];
				}
			}
		}	
	

	return final_support;
}
