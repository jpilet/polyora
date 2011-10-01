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
#ifndef VECMAP_H
#define VECMAP_H

#include <vector>
#include <algorithm>

template <typename _Key, typename _Data, typename _Compare = std::less<_Key> >
class vecmap 
{
public:

	typedef std::pair<_Key, _Data> value_type;
	typedef typename std::vector<value_type> sorted_vector;
	typedef typename sorted_vector::iterator iterator;

protected:

	sorted_vector svec;

	struct key_compare : _Compare {
		bool operator() (const value_type &a, const value_type &b) const {
			return _Compare::operator()(a.first, b.first);
		}
	};

	key_compare kcomp;
	_Compare comp;

public:

	iterator begin() { return svec.begin(); }
	iterator end() { return svec.end(); }
	unsigned size() { return svec.size(); }
	void clear() { svec.clear(); }
	void reserve(unsigned n) { svec.reserve(n); }

	iterator lower_bound(const _Key &k) {
		value_type v;
		v.first = k;
		return std::lower_bound(begin(), end(), v, kcomp);
	}

	iterator find(const _Key &k) {
		iterator it = lower_bound(k);
		if (it == end() || comp(k, it->first)) 
			return svec.end();
		return it;
	}

	_Data &operator[](const _Key &k) {

		iterator it = lower_bound(k);
		if (it == end() || comp(k, it->first)) {
			_Data d;
			it = svec.insert(it, value_type(k, d));
			return it->second;
		}
		return it->second;
	}

	std::pair<iterator,bool> insert(const value_type &val) {

		std::pair<iterator, bool> ret;
		iterator it = lower_bound(val->first);
		if (it == end() || comp(val->first, it->first)) {
			ret.first = insert(it, val);
			ret.second = true;
			return ret;
		}
		it->second = val->second;
		ret.first = it;
		ret.second = false;
		return ret;
	}

	void erase(const _Key &k) {
		iterator it = find(k);
		if (it!=end()) svec.erase(it);
	}
};

#endif
