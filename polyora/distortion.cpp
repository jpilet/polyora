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

#include "distortion.h"

#include <stdio.h>

DistortParams *DistortParams::load(const char *file) 
{
	if (file==0) return 0;

	FILE *f=fopen(file,"r");
	if (!f) {
		perror(file);
		return 0;
	}

	DistortParams *r = new DistortParams();

	int fields = fscanf(f,"%f%f%f%f%f%f%f%f",
			&r->fx, &r->fy, &r->cx, &r->cy,
			r->k, r->k+1, r->p, r->p+1);
	fclose(f);

	if (fields != 8) {
		fprintf(stderr,"%s: can't read calibration values.\n", file);
		delete r;
		return 0;
	}
	return r;
}

void distort(const float x, const float y, float distorted[2], const DistortParams &p) 
{
	float r2 = x*x + y*y;
	float d = 1+ p.k[0]*r2 +  p.k[1]*r2*r2;
	float xy= x*y;
	distorted[0] =  p.fx*(x * d + 2* p.p[0] *xy + p.p[1]*(r2+2*x*x)) + p.cx;
	distorted[1] =  p.fy*(y * d + 2* p.p[1] *xy + p.p[0]*(r2+2*y*y)) + p.cy;
}

void undistort(keypoint *kpt, const DistortParams &p)
{
	float uv[2];
	undistort(kpt->fr_u(), kpt->fr_v(), uv, p);
	kpt->u_undist = uv[0];
	kpt->v_undist = uv[1];
}

void undistort(const float _u, const float _v, float xy[2], const DistortParams &p)
{

	// Distorted coordinates, centered on principal point and focal-normalized.
	float u = (_u-p.cx)/p.fx;
	float v = (_v-p.cy)/p.fy;

	// We initialize the undistorted coordinates with the distorted ones
	float x = u;
	float y = v;

	// 6 iterations is the max, but more than 3 would be very unusual
	for (int i=0; i<5; i++) {

		float t1 = x*x;
		float t2 = p.k[0]*t1;
		float t3 = y*y;
		float t4 = p.k[0]*t3;
		float t6 = t1*t1;
		float t7 = p.k[1]*t6;
		float t8 = p.k[1]*t1;
		float t9 = t8*t3;
		float t10 = 6.0f*t9;
		float t11 = t3*t3;
		float t12 = p.k[1]*t11;
		float t14 = p.p[1]*x;
		float t16 = p.p[0]*y;
		float t21 = t1*x;
		float t25 = p.k[1]*p.k[1];
		float t27 = t11*t3;
		float t30 = t3*y;
		float t38 = t6*t1;
		float t62 = 1.0f+4.0f*t4+4.0f*t2+12.0f*p.k[0]*t21*p.p[1]+20.0f*t25*t1*t27+12.0f*p.k[0]*t30*
			p.p[0]+16.0f*p.k[1]*t11*y*p.p[0]+20.0f*t25*t38*t3+12.0f*t4*t14+16.0f*t7*t16+32.0f*p.k[1]*t21
			*t3*p.p[1]+32.0f*t8*t30*p.p[0]+16.0f*t12*t14+32.0f*t16*t14+12.0f*t9+30.0f*t25*t6*t11+6.0f
			*t12;
		float t65 = p.k[0]*p.k[0];
		float t70 = t6*t6;
		float t73 = t11*t11;
		float t76 = p.p[0]*p.p[0];
		float t81 = p.p[1]*p.p[1];
		float t108 = 8.0f*t14+8.0f*t16+3.0f*t65*t6+3.0f*t65*t11+5.0f*t25*t70+5.0f*t25*t73+
			12.0f*t76*t3-4.0f*t76*t1-4.0f*t81*t3+16.0f*p.k[1]*t6*x*p.p[1]+8.0f*p.k[0]*t27*p.k[1]
			+8.0f*p.k[0]*t38*p.k[1]+6.0f*t65*t1*t3+6.0f*t7+24.0f*t2*t12+12.0f*t81*t1+24.0f*p.k[0]*t6*p.k[1]*t3+
			12.0f*t2*t16;
		float t110 = 1.0f/(t62+t108);
		float t112 = t1+t3;
		float t114 = t112*t112;
		float t116 = 1.0f+p.k[0]*t112+p.k[1]*t114;
		float t118 = p.p[0]*x;
		float t124 = u-x*t116-2.0f*t118*y-p.p[1]*(3.0f*t1+t3);
		float t136 = (x*p.k[0]*y+2.0f*p.k[1]*y*t21+2.0f*x*p.k[1]*t30+t118+p.p[1]*y)*t110;
		float t143 = v-y*t116-2.0f*t14*y-p.p[0]*(t1+3.0f*t3);
		float dx = (1.0f+t2+3.0f*t4+t7+t10+5.0f*t12+2.0f*t14+6.0f*t16)*t110*t124-2.0f*
			t136*t143;
		float dy = -2.0f*t136*t124+(1.0f+3.0f*t2+t4+5.0f*t7+t10+t12+2.0f*t16+6.0f*t14)*
			t110*t143;
		x += dx;
		y += dy;
		if ((dx*dx+dy*dy)<(.01*.01)) break;
	}
	
	// Verification code
	/*
	float r2 = x*x + y*y;
	float d = 1+ k[0]*r2 + k[1]*r2*r2;
	float dst[2] = {
		fx*(x * d + 2*p[0] *x*y + p[1]*(r2+2*x*x))+cx,
		fy*(y * d + 2*p[1] *x*y + p[0]*(r2+2*y*y))+cy
	};
	cout << " [" << dst[0] << ", " << dst[1] << "] ";
	cout << _u << ", " << _v << " => " << x*fx+cx << ", " << y*fy+cy << "(" << i+1 << " iterations)" << endl;
	*/
	xy[0] = x;
	xy[1] = y;
}

