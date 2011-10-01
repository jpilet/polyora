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
/*
Copyright 2005, 2006 Computer Vision Lab, 
Ecole Polytechnique Federale de Lausanne (EPFL), Switzerland. 
All rights reserved.

This file is part of BazAR.

BazAR is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

BazAR is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
BazAR; if not, write to the Free Software Foundation, Inc., 51 Franklin
Street, Fifth Floor, Boston, MA 02110-1301, USA 
*/
#include <iostream>
#include <highgui.h>
#include "pyrimage.h"

using namespace std;

PyrImage::PyrImage(IplImage *im, int nblev, bool build_it) : nbLev(nblev)
{
  images = new IplImage *[nblev];

  images[0] = im;

  for (int i=1; i<nbLev; ++i) {
    images[i] = cvCreateImage(cvSize(images[i-1]->width/2, images[i-1]->height/2),
                              im->depth, im->nChannels);
  }

  if (build_it)  
      build();
}

PyrImage::~PyrImage() {
  for (int i=0; i<nbLev; ++i) 
    cvReleaseImage(&images[i]);
  delete [] images;
}

void PyrImage::build()
{
  for (int i=1; i<nbLev; ++i) 
    cvPyrDown(images[i-1], images[i]);
}

PyrImage *PyrImage::load(int level, const char *filename, int color, bool fatal) {

  IplImage *im = cvLoadImage(filename, color);

  if (im == 0) {
    cerr << filename << ": unable to load image.\n";
    if (fatal)
      exit(-1);
    return 0;
  }

  PyrImage *r= new PyrImage(im, level);
  r->build();
  return r;
}

int PyrImage::convCoord(int x, int from, int to, unsigned max)
{
  if (max == 2) 
    return (convCoord(x, from, to, 0) + convCoord(x, from, to, 1)) >> 1;
  if (to==from) 
    return x;
  if (to<from) {
    if (max == 1) {
      int r=x;
      for (int i=from; i<to; ++i) {
        r = r*2 + 1;
      }
      return r;
    } 
    return x << ( from - to );
  }
  return x >> ( to - from );
}

float PyrImage::convCoordf(float x, int from, int to)
{
  if (to == from) 
    return x;

  if (to<from) 
    return x * float(1 << (from-to));

  return x / float(1 << (to-from)); 
}

void PyrImage::setPixel(unsigned x, unsigned y, CvScalar &val)
{
  if (( x >= (unsigned)images[0]->width ) || ( y >= (unsigned)images[0]->height ))
    return;

  for (int i=0; i<nbLev; ++i) {
    cvSet2D(images[i], 
      convCoord((int)y, 0, i),
      convCoord((int)x, 0, i),
      val);
  }
}

void PyrImage::set(CvScalar val)
{
  for (int i=0; i<nbLev; ++i)
    cvSet(images[i], val);
}

PyrImage *PyrImage::clone() const
{
  PyrImage *p = new PyrImage(cvCloneImage(images[0]), nbLev);
  for (int i=1; i<nbLev; ++i)
    cvCopy(images[i], p->images[i]);
  return p;
}

void PyrImage::setImageROI(CvRect rect) 
{
  CvRect r;
  for (int i=0; i<nbLev; ++i) {
    r.x = convCoord(rect.x, 0, i);
    r.y = convCoord(rect.y, 0, i);
    r.width = convCoord(rect.width, 0, i);
    r.height = convCoord(rect.height, 0, i);
    cvSetImageROI(images[i], r);
  }
}

void PyrImage::swap(PyrImage &a) {
   assert(nbLev == a.nbLev);
   for (int i=0; i<nbLev; i++) {
      IplImage *tmp = a[i];
      a.images[i] = images[i];
      images[i] = tmp;
   }
}

void PyrImage::resetImageROI()
{
  for (int i=0; i<nbLev; ++i) {
    cvResetImageROI(images[i]);
  }
}


static inline void bicub_interp3f (
  float u,
  float v,
  float C1[4][4],
  float C2[4][4],
  float C3[4][4],
  double cgret[3])
{
	  float t8;
  float t25; float t26; float t10; float t14; float t24;
  float t4; float t37; float t1; float t39; float t18;
  float t5; float t22; float t2; float t3; float t63;
  float t23; float t51;
  t1 = v * v;
  t2 = t1 * v;
  t3 = 0.5e0f * t2;
  t4 = 0.5e0f * v;
  t5 = -t3 + t1 - t4;
  t8 = 0.15e1f * t2;
  t10 = t8 - 0.25e1f * t1 + 0.1e1f;
  t14 = -t8 + 0.2e1f * t1 + t4;
  t18 = t3 - 0.5e0f * t1;
  t22 = u * u;
  t23 = t22 * u;
  t24 = 0.5e0f * t23;
  t25 = 0.5e0f * u;
  t26 = -t24 + t22 - t25;
  t37 = 0.15e1f * t23;
  t39 = t37 - 0.25e1f * t22 + 0.1e1f;
  t51 = -t37 + 0.2e1f * t22 + t25;
  t63 = t24 - 0.5e0f * t22;
  cgret[0] = (t5 * C1[0][0] + t10 * C1[1][0] + t14 * C1[2][0] + t18 * C1[3][0]) * t26 
	  + (t5 * C1[0][1] + t10 * C1[1][1] + t14 * C1[2][1] + t18 * C1[3][1]) * t39 
	  + (t5 * C1[0][2] + t10 * C1[1][2] + t14 * C1[2][2] + t18 * C1[3][2]) * t51 
	  + (t5 * C1[0][3] + t10 * C1[1][3] + t14 * C1[2][3] + t18 * C1[3][3]) * t63;
  cgret[1] = (t5 * C2[0][0] + t10 * C2[1][0] + t14 * C2[2][0] + t18 * C2[3][0]) * t26 
	  + (t5 * C2[0][1] + t10 * C2[1][1] + t14 * C2[2][1] + t18 * C2[3][1]) * t39 
	  + (t5 * C2[0][2] + t10 * C2[1][2] + t14 * C2[2][2] + t18 * C2[3][2]) * t51 
	  + (t5 * C2[0][3] + t10 * C2[1][3] + t14 * C2[2][3] + t18 * C2[3][3]) * t63;
  cgret[2] = (t5 * C3[0][0] + t10 * C3[1][0] + t14 * C3[2][0] + t18 * C3[3][0]) * t26 
	  + (t5 * C3[0][1] + t10 * C3[1][1] + t14 * C3[2][1] + t18 * C3[3][1]) * t39 
	  + (t5 * C3[0][2] + t10 * C3[1][2] + t14 * C3[2][2] + t18 * C3[3][2]) * t51 
	  + (t5 * C3[0][3] + t10 * C3[1][3] + t14 * C3[2][3] + t18 * C3[3][3]) * t63;
}

static void bicub_interp3f_grad (
  float u,
  float v,
  float C1[4][4],
  float C2[4][4],
  float C3[4][4],
  double color[3],
  float *grad)
{
  float t14; float t44; float t15; float t23; float t92;
  float t67; float t35; float t11; float t149; float t8;
  float t46; float t48; float t147; float t25; float t189;
  float t62; float t197; float t195; float t119; float t201;
  float t199; float t211; float t209; float t18; float t143;
  float t145; float t213; float t50; float t151; float t100;
  float t131; float t1; float t141; float t215; float t139;
  float t27; float t203; float t69; float t137; float t221;
  float t2; float t135; float t19; float t219; float t36;
  float t223; float t225; float t155; float t52; float t227;
  float t3; float t133; float t129; float t40; float t29;
  float t10; float t90; float t71; float t4; float t42;
  float t121; float t5; float t127; float t54; float t205;
  float t21; float t58; float t31; float t65; float t74;
  float t125; float t6; float t38; float t153; float t56;
  float t33; float t117; float t77; float t217; float t123;
  float t80; float t191; float t207; float t81; float t108;
  float t83; float t82; float t22; float t193; float t60;
  t1 = v * v;
  t2 = t1 * v;
  t3 = 0.5e0f * t2;
  t4 = 0.5e0f * v;
  t5 = -t3 + t1 - t4;
  t6 = C1[0][0];
  t8 = 0.15e1f * t2;
  t10 = t8 - 0.25e1f * t1 + 0.1e1f;
  t11 = C1[1][0];
  t14 = -t8 + 0.2e1f * t1 + t4;
  t15 = C1[2][0];
  t18 = t3 - 0.5e0f * t1;
  t19 = C1[3][0];
  t21 = t5 * t6 + t10 * t11 + t14 * t15 + t18 * t19;
  t22 = u * u;
  t23 = 0.15e1f * t22;
  t25 = -t23 + 0.2e1f * u - 0.5e0f;
  t27 = C1[0][1];
  t29 = C1[1][1];
  t31 = C1[2][1];
  t33 = C1[3][1];
  t35 = t5 * t27 + t10 * t29 + t14 * t31 + t18 * t33;
  t36 = 0.45e1f * t22;
  t38 = t36 - 0.50e1f * u;
  t40 = C1[0][2];
  t42 = C1[1][2];
  t44 = C1[2][2];
  t46 = C1[3][2];
  t48 = t5 * t40 + t10 * t42 + t14 * t44 + t18 * t46;
  t50 = -t36 + 0.4e1f * u + 0.5e0f;
  t52 = C1[0][3];
  t54 = C1[1][3];
  t56 = C1[2][3];
  t58 = C1[3][3];
  t60 = t5 * t52 + t10 * t54 + t14 * t56 + t18 * t58;
  t62 = t23 - 0.10e1f * u;
  t65 = 0.15e1f * t1;
  t67 = -t65 + 0.2e1f * v - 0.5e0f;
  t69 = 0.45e1f * t1;
  t71 = t69 - 0.50e1f * v;
  t74 = -t69 + 0.4e1f * v + 0.5e0f;
  t77 = t65 - 0.10e1f * v;
  t80 = t22 * u;
  t81 = 0.5e0f * t80;
  t82 = 0.5e0f * u;
  t83 = -t81 + t22 - t82;
  t90 = 0.15e1f * t80;
  t92 = t90 - 0.25e1f * t22 + 0.1e1f;
  t100 = -t90 + 0.2e1f * t22 + t82;
  t108 = t81 - 0.5e0f * t22;
  t117 = C2[0][0];
  t119 = C2[1][0];
  t121 = C2[2][0];
  t123 = C2[3][0];
  t125 = t5 * t117 + t10 * t119 + t14 * t121 + t18 * t123;
  t127 = C2[0][1];
  t129 = C2[1][1];
  t131 = C2[2][1];
  t133 = C2[3][1];
  t135 = t5 * t127 + t10 * t129 + t14 * t131 + t18 * t133;
  t137 = C2[0][2];
  t139 = C2[1][2];
  t141 = C2[2][2];
  t143 = C2[3][2];
  t145 = t5 * t137 + t10 * t139 + t14 * t141 + t18 * t143;
  t147 = C2[0][3];
  t149 = C2[1][3];
  t151 = C2[2][3];
  t153 = C2[3][3];
  t155 = t5 * t147 + t10 * t149 + t14 * t151 + t18 * t153;
  t189 = C3[0][0];
  t191 = C3[1][0];
  t193 = C3[2][0];
  t195 = C3[3][0];
  t197 = t5 * t189 + t10 * t191 + t14 * t193 + t18 * t195;
  t199 = C3[0][1];
  t201 = C3[1][1];
  t203 = C3[2][1];
  t205 = C3[3][1];
  t207 = t5 * t199 + t10 * t201 + t14 * t203 + t18 * t205;
  t209 = C3[0][2];
  t211 = C3[1][2];
  t213 = C3[2][2];
  t215 = C3[3][2];
  t217 = t5 * t209 + t10 * t211 + t14 * t213 + t18 * t215;
  t219 = C3[0][3];
  t221 = C3[1][3];
  t223 = C3[2][3];
  t225 = C3[3][3];
  t227 = t5 * t219 + t10 * t221 + t14 * t223 + t18 * t225;

  grad[0] = t21 * t25 + t35 * t38 + t48 * t50 + t60 * t62;
  grad[1] = (t67 * t6 + t71 * t11 + t74 * t15 + t77 * t19) * t83 + (t67 * t27 + t71 * t29 + t74 * t31 + t77 * t33) * t92 + (t67 * t40 + t71 * t42 + t74 * t44 + t77 * t46) * t100 + (t67 * t52 + t71 * t54 + t74 * t56 + t77 * t58) * t108;

  grad[1*2] = t125 * t25 + t135 * t38 + t145 * t50 + t155 * t62;
  grad[1*2+1] = (t67 * t117 + t71 * t119 + t74 * t121 + t77 * t123) * t83 + (t67 * t127 + t71 * t129 + t74 * t131 + t77 * t133) * t92 + (t67 * t137 + t71 * t139 + t74 * t141 + t77 * t143) * t100 + (t67 * t147 + t71 * t149 + t74 * t151 + t77 * t153) * t108;

  grad[2*2+0] = t197 * t25 + t207 * t38 + t217 * t50 + t227 * t62;
  grad[2*2+1] = (t67 * t189 + t71 * t191 + t74 * t193 + t77 * t195) * t83 + (t67 * t199 + t71 * t201 + t74 * t203 + t77 * t205) * t92 + (t67 * t209 + t71 * t211 + t74 * t213 + t77 * t215) * t100 + (t67 * t219 + t71 * t221 + t74 * t223 + t77 * t225) * t108;
  color[0] = t21 * t83 + t35 * t92 + t48 * t100 + t60 * t108;
  color[1] = t125 * t83 + t135 * t92 + t145 * t100 + t155 * t108;
  color[2] = t197 * t83 + t207 * t92 + t217 * t100 + t227 * t108;
}

static CvScalar cvBicubicSample(IplImage *im, float x, float y, float *grad=0) {
	float ixf = (float)floorf(x);
	float iyf = (float)floorf(y);
	int ix = (int) ixf;
	int iy = (int) iyf;

	CvScalar rgb;
	if((ix < 0) || (ix > im->width) || (iy<0) || (iy>im->height)) 
		return cvScalarAll(0); 

	float u = x-ixf;
	float v = y-iyf;
	float C[3][4][4];

	if((ix <= 0) || (ix >= im->width-2) || (iy<=0) || (iy>=im->height-2)) {
		for (int y=0;y<4;y++) {
			int sy = MIN(im->height-1,MAX(0, iy-1+y));
			for (int x=0;x<4;x++) {
				int sx = 3*(MIN(im->width-1,MAX(0, ix-1+x)));
				for (int c=0;c<3;c++)
					C[c][y][x] = CV_IMAGE_ELEM(im, unsigned char, sy,sx+c);
			}
		}
	} else {
		unsigned char *src = &CV_IMAGE_ELEM(im, unsigned char, (iy-1),(ix-1)*3);
		for (int y=0;y<4;y++) {
			unsigned char *s=src;
			for (int x=0;x<4;x++) {
				for (int c=0;c<3;c++)
					C[c][y][x] = *s++;
			}
			src += im->widthStep;
		}
	}

	if (grad)
		bicub_interp3f_grad(u,v,C[0],C[1],C[2],rgb.val, grad);
	else
		bicub_interp3f(u,v,C[0],C[1],C[2],rgb.val);
	return rgb;
}

CvScalar PyrImage::bicubicSample(float _u, float _v, int l, float *grad) const {
	float div = 1.0f/(float)(1<<l);
	float x = _u*div;
	float y = _v*div;
	CvScalar s= cvBicubicSample(images[l], x,y,grad);
	if (grad) {
		for (int i=0;i<6;i++) grad[i]*=div;
	}
	return s;
}


CvScalar PyrImage::randomSample(float u, float v, float sigma2, float *grad) const
{
	int l=-1;

	float sigma_l2;
	float sigma_l_p2;

	do {
		l++;
		sigma_l2 = ((1<<(2*l))-1)/3.0f;
		sigma_l_p2 = ((1<<(2*l+2))-1)/3.0f;

		if (l==nbLev-2) break;
	} while (!((sigma_l2 <= sigma2) && (sigma_l_p2 >= sigma2)));

	float sdiv = 1.0f/(sigma_l_p2-sigma_l2);
	float rs = (sigma2 - sigma_l2)*sdiv;

	if (rs<0) rs=0;
	if (rs>1) rs=1;

	float grad1[3][2];
	float grad2[3][2];
	float *g1=0;
	float *g2=0;
	if (grad) {
		g1=&grad1[0][0];
		g2=&grad2[0][0];
	}
	CvScalar r;
	CvScalar a = bicubicSample(u,v,l,g1);
	CvScalar b = bicubicSample(u,v,l+1,g2); 
	float mrs = 1-rs;
	for (int i=0;i<3;i++) {
		float d = (float)(b.val[i]-a.val[i]);
		float v = (float)(a.val[i]+rs*d);
		if (grad) {
			grad[i*3 + 0] = mrs*g1[i*2] +rs*g2[i*2];
			grad[i*3 + 1] = mrs*g1[i*2+1] + rs*g2[i*2+1];
			grad[i*3 + 2] = d*sdiv;
		}
		v = MAX(v,0);
		r.val[i] = MIN(v,255);
	}
	return r;
}

/*
CvScalar PyrImage::randomSample(float u, float v, float sigma2, float *grad)
{
	int l=-1;

	float sigma_l2;
	float sigma_l_p2;

	do {
		l++;
		sigma_l2 = ((1<<(2*l))-1)/3.0;
		sigma_l_p2 = ((1<<(2*l+2))-1)/3.0;

		if (l==nbLev-2) break;
	} while (!((sigma_l2 <= sigma2) && (sigma_l_p2 >= sigma2)));

	float rs = (sigma2 - sigma_l2)/(sigma_l_p2-sigma_l2);

	if (rs<0) rs=0;
	if (rs>1) rs=1;

	CvScalar r;
	CvScalar a = bicubicSample(u,v,l);
	CvScalar b = bicubicSample(u,v,l+1); 
	for (int i=0;i<3;i++) {
		double v = (1-rs)*a.val[i] + rs*b.val[i];
		v = MAX(v,0);
		r.val[i] = MIN(v,255);
	}
	return r;
}

*/
