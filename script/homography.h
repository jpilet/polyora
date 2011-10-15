#ifndef CMPHOMO_H
#define CMPHOMO_H

namespace script_homography {

void homography_transform(const float a[2], const float H[3][3], float r[2]);

/* computes the homography sending [0,0] , [0,1], [1,1] and [1,0]
 * to x,y,z and w.
 */
void homography_from_4pt(const float *x, const float *y, const float *z, const float *w, float cgret[9]);

/*
 * computes the homography sending a,b,c,d to x,y,z,w
 */
void homography_from_4corresp(
		const float *a, const float *b, const float *c, const float *d,
		const float *x, const float *y, const float *z, const float *w, float R[3][3]);

/*
 * Compute the inverse homography and returns the determinant of m.
 */
float homography_inverse(const float m[3][3], float dst[3][3]);

}  // namespace script_homography

#endif
