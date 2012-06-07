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
#include "kpttracker.h"
#ifndef KMEANTREE_H
#define KMEANTREE_H

#include <vector>
#include <stdio.h>
#include <map>
#include "sqlite3.h"

namespace kmean_tree {

	static const unsigned nb_branches = 4;
#ifdef WITH_PATCH_TAGGER_DESCRIPTOR
#ifdef WITH_PATCH_AS_DESCRIPTOR
        static const unsigned descriptor_size=256; // use image patch as descriptor
#endif
#ifdef WITH_PCA_DESCRIPTOR
        static const unsigned descriptor_size=32; // number of PCA modes to use.
#endif
#endif

	struct descriptor_t {
		float descriptor[descriptor_size];
	};
	typedef std::vector<descriptor_t *> descriptor_array;

	class mean_t {
	public:
		float mean[descriptor_size];

		mean_t();

	       void accumulate(float a, float b, descriptor_t *d);
	       double distance(descriptor_t *d);
	};

	class node_t {
	public: 

		mean_t mean;
		unsigned id; 

		node_t *clusters[nb_branches];

		descriptor_array data;

		node_t();

		bool is_leaf() {
		       	for (unsigned i=0; i<nb_branches; i++) 
				if (clusters[i]) return false; 
			return true; 
		}

		unsigned count_leafs() {
			unsigned total=0;
		       	for (unsigned i=0; i<nb_branches; i++) 
				if (clusters[i]) total += clusters[i]->count_leafs();
			if (total==0) return 1;
			return total; 
		}

		void recursive_split(int max_level, int min_elem, int level=0);
		bool run_and_split();

		bool save(const char *filename);

		unsigned get_id(descriptor_t *d, node_t ** node=0, int depth=0);
		void print_summary(int depth=0);
		void cmp_scores(std::multimap<double,int> &scores, descriptor_t *data);

		bool load(FILE *f);
		bool save(FILE *f);

		//! open a sqlite3 database to store the node and its children.
		//! Clears previously stored tree.
		bool save_to_database(const char *fn);

		void assign_leaf_ids(unsigned *id_ptr);
	protected:
		void run_kmean(int nb_iter=32);
		bool save(sqlite3 *db, sqlite3_stmt *insert_node, sqlite3_stmt *insert_child);
	};


	//! build a tree from descriptors saved in a file
	node_t * build_from_data(const char *filename, int max_level, int min_elem, int stop);

	//! load a tree from a sqlite3 database
	node_t *load(sqlite3 *db);

	//! load a tree from a file
	node_t * load(const char *filename); 

	struct descr_file_packet {
		long ptr;
		descriptor_t d;
	};
};

#endif
