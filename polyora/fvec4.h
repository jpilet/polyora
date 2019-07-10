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
#ifndef FVEC4_H
#define FVEC4_H

#include <math.h>
#include <algorithm>
#include <xmmintrin.h>
#include <emmintrin.h>

//! Represent 4 integers of 32bits each for SIMD operations 
struct vec4_32i {
	__m128i data;

	vec4_32i(){}
	vec4_32i(__m128i a) { data=a; }
	vec4_32i(int a) { _mm_set1_epi32(a); }
	vec4_32i(int a, int b, int c, int d) { _mm_set_epi32(a,b,c,d); }
};

//! Represent 4 float values for SIMD operations.
struct fvec4 {
	__m128 data;
	static const int size = 4;

	fvec4() {}
	fvec4(__m128 a) {data=a;}
	fvec4(const fvec4 &a) {data=a.data;}
	fvec4(float a) {data = _mm_set_ps1(a);}
	fvec4(float a, float b, float c, float d) { data = _mm_set_ps(a,b,c,d); }

	fvec4 operator = (const fvec4 &a) { data = a.data; return *this; }
	fvec4 operator = (const __m128 a) { data = a; return *this; }
	fvec4 operator = (float a) { data = _mm_set_ps1(a); return *this; }

	fvec4 operator += (const fvec4 &a) {  return *this = _mm_add_ps(data,a.data); }
	fvec4 operator -= (const fvec4 &a) {  return *this = _mm_sub_ps(data,a.data); }
	fvec4 operator *= (const fvec4 &a) {  return *this = _mm_mul_ps(data,a.data); }
	fvec4 operator /= (const fvec4 &a) {  return *this = _mm_div_ps(data,a.data); }
	float &operator [] (int i) { return ((float *) &data)[i]; }
	const float &operator [] (int i) const { return ((const float *) &data)[i]; }

	operator vec4_32i() { return vec4_32i(_mm_cvttps_epi32(data)); }

	float horizontal_max() const { 
		return std::max( 
			std::max( (*this)[0], (*this)[1] ),
			std::max( (*this)[2], (*this)[3] ));
	}

	float horizontal_sum() const {
		const __m128 t = _mm_add_ps(data, _mm_movehl_ps(data, data));
		float result;
		_mm_store_ss(&result, _mm_add_ss(t, _mm_shuffle_ps(t, t, 1)));
		return result;
	}

	int horizontal_max_index() { 
		int best=0;
		float val = (*this)[0];
		for (int i=1; i<4; i++) {
			if (val < (*this)[i]) {
				val = (*this)[i];
				best = i;
			}
		}
		return best;
	}
};

inline fvec4 operator + (const fvec4 &a, const fvec4 &b) { return fvec4(_mm_add_ps(a.data, b.data)); }
inline fvec4 operator - (const fvec4 &a, const fvec4 &b) { return fvec4(_mm_sub_ps(a.data, b.data)); }
inline fvec4 operator - (const fvec4 &a) { return fvec4(0) - a; }
inline fvec4 operator * (const fvec4 &a, const fvec4 &b) { return fvec4(_mm_mul_ps(a.data, b.data)); }
inline fvec4 operator / (const fvec4 &a, const fvec4 &b) { return fvec4(_mm_div_ps(a.data, b.data)); }
inline fvec4 rcp(fvec4 &a) { return fvec4(_mm_rcp_ps(a.data)); }
inline fvec4 operator < (const fvec4 &a, const fvec4 &b) { return fvec4(_mm_cmplt_ps(a.data, b.data)); }
inline fvec4 operator > (const fvec4 &a, const fvec4 &b) { return fvec4(_mm_cmpgt_ps(a.data, b.data)); }
inline fvec4 operator <= (const fvec4 &a, const fvec4 &b) { return fvec4(_mm_cmpngt_ps(a.data, b.data)); }
inline fvec4 operator >= (const fvec4 &a, const fvec4 &b) { return fvec4(_mm_cmpnlt_ps(a.data, b.data)); }
inline fvec4 operator == (const fvec4 &a, const fvec4 &b) { return fvec4(_mm_cmpeq_ps(a.data, b.data)); }

inline fvec4 operator & (const fvec4 &a, const fvec4 &b) { return fvec4(_mm_and_ps(a.data, b.data)); }
inline fvec4 operator | (const fvec4 &a, const fvec4 &b) { return fvec4(_mm_or_ps(a.data, b.data)); }
inline fvec4 operator ^ (const fvec4 &a, const fvec4 &b) { return fvec4(_mm_xor_ps(a.data, b.data)); }
inline fvec4 min(const fvec4 &a, const fvec4 &b) { return fvec4(_mm_min_ps(a.data, b.data)); }
inline fvec4 max(const fvec4 &a, const fvec4 &b) { return fvec4(_mm_max_ps(a.data, b.data)); }

inline fvec4 loadu(const float *ptr) {
    return fvec4(_mm_loadu_ps(ptr));
}

inline void storeu(const fvec4 &value, fvec4 *dest) {
    _mm_storeu_ps((float *) dest, value.data);
}

#endif
