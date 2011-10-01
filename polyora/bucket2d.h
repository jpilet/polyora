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
#ifndef BUCKET2D_H
#define BUCKET2D_H

#include <assert.h>
#include <math.h>
#include <string.h>
#include "mlist.h"

/*! Fast access to 2d points within a limited range.
 
  \ingroup TracksGroup

  The template argument T takes any class that contain a member
  points_in_frame such as the one in the example below:

  \verbatim
  class bucket_elem {
  	float u, v;
	mlist_elem<bucket_elem> points_in_frame;
  };
  \endverbatim
*/
template <typename T>
class bucket2d {
public:

	bucket2d() : buckets(0), nb_elem(0) {}
	bucket2d(unsigned width, unsigned height, unsigned size_bits):buckets(0),nb_elem(0) {setup(width,height,size_bits);}
	virtual ~bucket2d();

	void setup(unsigned width, unsigned height, unsigned size_bits);

	void add_pt(T *p) { assert(p); ++nb_elem; MLIST_INSERT(buckets[idx(p->u, p->v)], p, points_in_frame); }

	void clear();

	void rm_pt(T *p) { --nb_elem; MLIST_RM(&buckets[idx(p->u, p->v)], p, points_in_frame); }

	void move_pt(T *p, float u, float v) {
		unsigned old_idx =  idx(p->u,p->v);
		unsigned new_idx = idx(u,v);
		if (old_idx != new_idx) {
			MLIST_RM(&buckets[old_idx],p,points_in_frame);
			MLIST_INSERT(buckets[new_idx], p, points_in_frame);
		}
		p->u=u;
		p->v=v;
	}

	class iterator {
	public:
		iterator(bucket2d &b, float u, float v, float r);
		iterator(bucket2d &b);
		void operator ++();
		T *operator *() { return it; }
		T *elem() { return it; }
		bool end() { return j>ve; }

	protected:
		bucket2d *b;

		unsigned cur_idx, ub, ue, vb, ve, i, j;
		T *it;
		void next();
	};

	iterator search(float u, float v, float r) { return iterator(*this, u, v, r); }
	iterator begin() { return iterator(*this); }
	T *closest_point(float u, float v, float max_dist);

	unsigned size() const  { return nb_elem; }

protected:
	unsigned size_bits;
	unsigned buckets_max_u, buckets_max_v;

	T **buckets;
	unsigned nb_elem;

	unsigned  idxu(unsigned u) {
		unsigned iu = u >> size_bits;
		if (iu>buckets_max_u) iu = buckets_max_u;
		return iu;
	}
	unsigned idxu(int u) { if (u<0) return 0; return idxu((unsigned)u); }

	unsigned idxv(unsigned v) {
		unsigned iv = v >> size_bits;
		if (iv>buckets_max_v) iv = buckets_max_v;
		return iv;
	}
	unsigned idxv(int v) {
	       	if (v<0) return 0; 
		return idxv((unsigned)v); 
	}
	unsigned idx(unsigned u, unsigned v) { return idxv(v)*(buckets_max_u+1) + idxu(u); }
	unsigned idx(int u, int v) { return idxv(v)*(buckets_max_u+1) + idxu(u); }
	unsigned idx(float u, float v) { return idx((int)u, (int)v); }
	//unsigned idx(float u, float v) { return idx((int)floorf(u), (int)floorf(v)); }
};

template<typename T>
bucket2d<T>::~bucket2d() {
	if (buckets) delete[] buckets;
}

template<typename T>
void bucket2d<T>::setup(unsigned width, unsigned height, unsigned size_bits)
{
	this->size_bits = size_bits;

	buckets_max_u = ((width + (1<<size_bits)-1) >> size_bits)-1;
	buckets_max_v = ((height + (1<<size_bits)-1) >> size_bits)-1;

	assert(buckets_max_u>0 && buckets_max_v>0);

	if (buckets) delete[] buckets;
	int n= (buckets_max_u+1)*(buckets_max_v+1);
	buckets = new T *[n];
	memset(buckets, 0, n*sizeof(T *));
	nb_elem = 0;
}

template<typename T>
void bucket2d<T>::clear() {
	if (!buckets) return;

	int n= (buckets_max_u+1)*(buckets_max_v+1);
	memset(buckets, 0, n*sizeof(T *));
	nb_elem= 0;
}

template<typename T>
bucket2d<T>::iterator::iterator(bucket2d &bucket, float u, float v, float r) 
	: b(&bucket)
{
	ub = b->idxu((int)floorf(u-r));
	ue = b->idxu((int)ceilf(u+r));
	vb = b->idxv((int)floorf(v-r));
	ve = b->idxv((int)ceilf(v+r));
	i = ub;
	j = vb;

	cur_idx = j*(b->buckets_max_u+1)+i;
	assert(cur_idx<(b->buckets_max_u+1)*(b->buckets_max_v+1));
	it = b->buckets[cur_idx];
	next();
}

template<typename T>
bucket2d<T>::iterator::iterator(bucket2d &bucket)
	: b(&bucket)
{
	ub = 0;
	ue = b->buckets_max_u;
	vb = 0;
	ve = b->buckets_max_v;
	i = 0;
	j = 0;

	cur_idx = 0;
	it = b->buckets[cur_idx];
	next();
}

template<typename T>
void bucket2d<T>::iterator::next() {
	while(it==0) {
		++i;
		if (i>ue) {
			i = ub;
			++j;
			if (j>ve) {
				cur_idx=0xffffffff;
				break;
			}
		}
		cur_idx = j*(b->buckets_max_u+1)+i;
		assert(cur_idx<(b->buckets_max_u+1)*(b->buckets_max_v+1));
		it = b->buckets[cur_idx];
	}
}

template<typename T>
void bucket2d<T>::iterator::operator ++() {
	it = it->points_in_frame.next;
	next();
}

template <typename T>
T *bucket2d<T>::closest_point(float u, float v, float max_dist)
{
	float dist = max_dist*max_dist;
	T* best=0;
	for(iterator it = search(u,v,max_dist); !it.end(); ++it) {
		float du = u - it.elem()->u;
		du *= du;
		if (du>max_dist) continue;
		float dv = v - it.elem()->v;
		dv *= dv;
		float d = du+dv;
		if (d<dist) {
			dist = d;
			best = it.elem();
		}
	}
	return best;
}

#endif
