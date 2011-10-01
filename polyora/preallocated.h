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
#ifndef PREALLOCATED_H
#define PREALLOCATED_H

template <class T, unsigned N>
class pre_allocated_alloc {
	public:
		// type definitions
		typedef T        value_type;
		typedef T*       pointer;
		typedef const T* const_pointer;
		typedef T&       reference;
		typedef const T& const_reference;
		typedef std::size_t    size_type;
		typedef std::ptrdiff_t difference_type;

		unsigned char _memory[N];
		unsigned next;

		// rebind allocator to type U
		template <class U>
		struct rebind {
			typedef pre_allocated_alloc<U, N> other;
		};

		// return address of values
		pointer address (reference value) const {
			return &value;
		}
		const_pointer address (const_reference value) const {
			return &value;
		}

		pre_allocated_alloc () throw() {
			next=0;
		}
		pre_allocated_alloc (const pre_allocated_alloc&) throw() {
			next=0;
		}

		template <class U>
		pre_allocated_alloc (const pre_allocated_alloc<U,N>&) throw() {
			next=0;
		}

		// return maximum number of elements that can be allocated
		size_type max_size () const throw() {
			return (N-next) / sizeof(T);
		}

		// allocate but don't initialize num elements of type T
		pointer allocate (size_type num, const void* = 0) {
			unsigned size = num*sizeof(T);
			unsigned end = next+size;
			if (end>N)
				std::__throw_bad_alloc();
			next = end;

			return (pointer)(_memory + next);
		}

		// initialize elements of allocated storage p with value value
		void construct (pointer p, const T& value) {
			// initialize memory with placement new
			new((void*)p)T(value);
		}

		// destroy elements of initialized storage p
		void destroy (pointer p) {
			// destroy objects by calling their destructor
			p->~T();
		}

		// deallocate storage p of deleted elements
		void deallocate (pointer , size_type ) {
		}
};

#endif

