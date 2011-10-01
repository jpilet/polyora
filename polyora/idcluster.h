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
#ifndef IDCLUSTER_H
#define IDCLUSTER_H

#include <stdio.h>
#include <map>
#include <set>
#include "vecmap.h"
#include "preallocated.h"
#include "sqlite3.h"

typedef unsigned cluster_id_t;

/*! Histogram. 
 * The name of this class is confusing. It is just a histogram, stored in a sparse manner.
 */
class id_cluster {
public:

	id_cluster() : total(0), id(0), weighted_sum(0) {}
	virtual ~id_cluster() {}

	typedef std::map<unsigned, unsigned> uumap;
	uumap histo;
	unsigned total;
	cluster_id_t id;
	float weighted_sum;

	// return the total cumulated in the bin 'id', after adding 'amount'.
	unsigned add(unsigned id, int amount=1);
	float dotprod(const id_cluster &a) const;
	void print() const;
	void clear();

	unsigned get_freq(unsigned id);
	float get_proba(unsigned id) { return (float)get_freq(id)/(float)total; }
};

/*
// malloc/free optimization
typedef std::pair<const id_cluster *, float> cluster_score;
struct cluster_score_cmp {
bool operator()( const id_cluster *a, const id_cluster *b) const { return a < b; }
};
typedef std::map<id_cluster *, double, cluster_score_cmp, pre_allocated_alloc<cluster_score, 512*1024> >
cluster_score_map;
*/
typedef std::map<id_cluster *, double> cluster_score_map;

class id_cluster_collection {
public:

	typedef std::set<id_cluster *> cluster_set;
	//typedef std::map<id_cluster *, float> cluster_map;
	typedef vecmap<id_cluster *, float> cluster_map;
	//typedef vecmap<unsigned, cluster_map> id2cluster_map;
	typedef std::map<unsigned, cluster_map> id2cluster_map;

	enum query_flags { QUERY_FREQ=0, QUERY_NORMALIZED_FREQ=1, QUERY_IDF=2, QUERY_MIN_FREQ=4, QUERY_BIN_FREQ = 8, QUERY_IDF_NORMALIZED=3 };
	id_cluster_collection(query_flags flags);
	virtual ~id_cluster_collection();

	void reduce(float threshold);

	void add_cluster(id_cluster *c);
	void remove_cluster(id_cluster *c);
	void update_cluster(id_cluster *c, unsigned id, int amount);

	// Merges b into a and deletes b 
	void merge_clusters(id_cluster *a, id_cluster *b);

	void cmp_best_clusters();
	unsigned get_best_cluster(unsigned id);
	id_cluster *get_best_cluster(id_cluster *c, float *dist);


	void print();
	bool save(const char *fn);
	bool save(FILE *f);
	bool save(sqlite3 *db, const char *tablename=0);
	bool load(const char *fn);
	bool load(sqlite3 *db, const char *tablename=0);
	void clear();
	float idf(unsigned id);
	float idf(id2cluster_map::iterator &id_it);

	cluster_set clusters;

	//! id2cluster[i][j].second contains the value of dimension i of the j'th vector with non-0 value at dim i.
	id2cluster_map id2cluster;
	std::map<unsigned, id_cluster *> best_cluster;

protected:

	struct cluster_dist_t {
		id_cluster *a, *b;
		float d;

		cluster_dist_t(id_cluster *a, id_cluster *b, float d);
		bool operator < (const cluster_dist_t &c) const;
	};


	std::set<cluster_dist_t> distance_matrix;

public:
	void get_scores(id_cluster *c, cluster_score_map &scores, id_cluster **best_c=0, float *_best_s=0);

	void set_query_rules(query_flags flags);

protected:
	void build_distance_matrix(float threshold);
	void add_to_distance_matrix(id_cluster *c, float threshold);
	void remove_from_distance_matrix(id_cluster *c);

	int version;
public:
	int get_version() const { return version; }

private:
	query_flags flags;
	bool is_idf_normalized;

	friend class incremental_query;
};

class incremental_query {
public:

	struct ranked_cluster {
		id_cluster * const c;
		const float score;
		ranked_cluster(id_cluster *c, float s) : c(c), score(s) {}

		bool operator< (const ranked_cluster &a) const { 
			if (a.score < score) return true;
			if (score < a.score) return false;
			return c<a.c;
		}
	};
	typedef std::set<ranked_cluster> ranked_cluster_set;
	typedef ranked_cluster_set::iterator iterator;
	ranked_cluster_set results;

	//! Constructor. One option is to use the create_incremental_query() of visual_database.
	incremental_query(id_cluster_collection *db);

	//! Update the query. The amount can be negative for removal.
	void modify(unsigned id, int amount=1);

	/*! Set the query to c. 
	 * Incremental or full query is done, depending on which one is faster.
	 */
	void set(id_cluster *c); 

	iterator sort_results(unsigned max_results=1);
	iterator sort_results_min_ratio(float ratio);

	iterator begin() { return results.begin(); }
	iterator end() { return results.end(); }

	void clear();
	id_cluster *get_best(float *score);

	// maintains non-zero, un-normalied dot product between 'query_cluster' and all clusters in 'database'.
	cluster_score_map scores;
	id_cluster query_cluster;
	id_cluster_collection *database;


	id_cluster_collection::query_flags get_flags() { return database->flags; }
	//void switch_flag(id_cluster_collection::query_flags flag) { clear(); database->set_query_rules(get_flags() ^ flag); }
	//void set_flag(id_cluster_collection::query_flags flag) { clear(); database->set_query_rules(get_flags() | flag); }
	void set_all_flags(id_cluster_collection::query_flags flag) { clear(); database->set_query_rules(flag); }

	int version;
protected:
	int flags;
};


#endif

