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
#ifndef MLIST_H
#define MLIST_H

#include <assert.h>

template <typename T>
struct mlist_elem {
	T *prev;
	T *next;

	mlist_elem() : prev(0), next(0) {}
};

#define MLIST_RM(root, p, m) mlist_rm(((p)->m.prev ? &(p)->m.prev->m : 0), \
		&(p)->m, \
	       	((p)->m.next ? &(p)->m.next->m : 0), \
	       	p,\
	       	root)
#define MLIST_INSERT(root, p, m) \
{ \
	if (root) root->m.prev = p; \
	p->m.next = root; \
	root = p; \
}

template<typename T>
void mlist_rm(mlist_elem<T> *prev, mlist_elem<T> *e, mlist_elem<T> *next, const T *p, T **root_ptr)
{
	if (prev==0 && root_ptr) {
		assert(p==*root_ptr);
	}
	if (root_ptr && (p == *root_ptr)) *root_ptr = e->next;
	if (prev) prev->next = e->next;
	if (next) next->prev = e->prev;
	e->next = e->prev = 0;
}

template<typename T>
void mlist_rm(mlist_elem<T> *prev, mlist_elem<T> *e, mlist_elem<T> *next, const T *p, int )
{
	mlist_rm<T>(prev,e,next,p,(T **) 0);
}

#define RLIST_RM(root, p, m) rlist_rm(((p)->m.prev ? &(p)->m.prev->m : 0), \
		&(p)->m, \
	       	((p)->m.next ? &(p)->m.next->m : 0), \
	       	p,\
	       	root)
#define RLIST_INSERT(root, p, m) \
{ \
	if (root) root->m.next = p; \
	p->m.prev = root; \
	root = p; \
}

template<typename T>
void rlist_rm(mlist_elem<T> *prev, mlist_elem<T> *e, mlist_elem<T> *next, const T *p, T **root_ptr)
{
	if (next==0 && root_ptr) {
		assert(p==*root_ptr);
	}
	if (root_ptr && (p == *root_ptr)) *root_ptr = e->prev;
	if (prev) prev->next = e->next;
	if (next) next->prev = e->prev;
	e->next = e->prev = 0;
}

template<typename T>
void rlist_rm(mlist_elem<T> *prev, mlist_elem<T> *e, mlist_elem<T> *next, const T *p, int )
{
	rlist_rm<T>(prev,e,next,p,(T **) 0);
}


#endif
