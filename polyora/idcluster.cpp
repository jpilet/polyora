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
#include <assert.h>
#include "idcluster.h"
#include <iostream>
#include <stdio.h>
#include <math.h>
#include <vector>

using namespace std;
//using namespace __gnu_cxx;

void id_cluster::clear()
{
	histo.clear();
	total=0;
	weighted_sum=0;
	id=0;
}

id_cluster_collection::id_cluster_collection(id_cluster_collection::query_flags flags) : flags(flags), version(0)
{
	is_idf_normalized=false;
}

void id_cluster_collection::merge_clusters(id_cluster *a, id_cluster *b)
{
	//cout << "merging " << a->id << " and " << b->id << endl;
	for (id_cluster::uumap::iterator it = b->histo.begin(); it!=b->histo.end(); ++it)
		a->add(it->first, it->second);
	remove_cluster(b);
	delete b;

	// update the id2cluster map
	for (id_cluster::uumap::iterator it = a->histo.begin(); it!=a->histo.end(); ++it) {
		id2cluster[it->first][a] = (float)a->histo[it->first]; // /(float)a->total;
	}
	is_idf_normalized = false;
	version++;
}

id_cluster *id_cluster_collection::get_best_cluster(id_cluster *c, float *dist)
{
	id_cluster *r;
	cluster_score_map scores;
	get_scores(c, scores, &r, dist);
	//retrieve(c, scores, &r, dist);
	return r;
}

void id_cluster_collection::set_query_rules(id_cluster_collection::query_flags _flags)
{
	if (is_idf_normalized && _flags == flags) return;
	flags = _flags;
	is_idf_normalized = true;

	for (cluster_set::iterator it(clusters.begin()); it!=clusters.end(); it++) 
	{
		float * sum = const_cast<float *>(&((*it)->weighted_sum));
		*sum = 0;

		if ( flags & (QUERY_BIN_FREQ | QUERY_IDF)) {
			for (id_cluster::uumap::iterator bin((*it)->histo.begin()); bin != (*it)->histo.end(); ++bin)
			{
				*sum += ((flags&QUERY_BIN_FREQ) ? 1 : bin->second) 
					* ((flags&QUERY_IDF) ? idf(bin->first) : 1);
			}
		} else {
			*sum = (*it)->total;
		}
	}
	/*
	for (id2cluster_map::iterator it(id2cluster.begin()); it!=id2cluster.end(); ++it) {
		for (cluster_map::iterator j(it->second.begin()); j!=it->second.end(); ++j) {
			j->second = j->first->histo[it->first] / j->first->weighted_sum; 
		}
	}
	*/
}

void id_cluster_collection::get_scores(id_cluster *c, cluster_score_map &scores, id_cluster **best_c, float *_best_s)
{
	if (best_c) *best_c = 0;
	double best_s=0;

	set_query_rules(flags);

	for (id_cluster::uumap::iterator it = c->histo.begin(); it!=c->histo.end(); ++it)
	{
		id2cluster_map::iterator id_it = id2cluster.find(it->first);
		double w_e = it->second;
		
		if (flags & QUERY_NORMALIZED_FREQ)
			w_e = w_e / (double)c->total;

		if (flags & QUERY_IDF)
			w_e *= idf(id_it);

		if (id_it!=id2cluster.end()) {
			for (cluster_map::iterator cit=id_it->second.begin(); cit!=id_it->second.end(); ++cit) {
				if (cit->first != c) {

					double n = cit->second;
					if (flags & QUERY_NORMALIZED_FREQ)
						n = n / (double)cit->first->total;
					assert(n>0);
					double p = w_e * n;

					cluster_score_map::iterator __i = scores.lower_bound(cit->first);
					if (__i == scores.end() || cit->first<(*__i).first) {
						scores.insert(__i, pair<id_cluster *, double>(cit->first, p));
					} else {
						p = __i->second += p;
					}
					if (best_c && p > best_s) {
						best_s = p;
						*best_c = cit->first;
					}
				}
			}
		}
	}

	if (best_c) {
		if (_best_s) {
		       	*_best_s = best_s;
			assert(*_best_s>=0 && *_best_s<=1);
		}
	}
}

void id_cluster_collection::add_cluster(id_cluster *c)
{
	version++;
	is_idf_normalized=false;

	if (clusters.insert(c).second) c->id = clusters.size();
	assert(c->id!=0);

	// update the id2cluster map
	for (id_cluster::uumap::iterator it = c->histo.begin(); it!=c->histo.end(); ++it) {
		id2cluster[it->first][c] = (float)c->histo[it->first]; 
		//id2cluster[it->first][c] = (float)c->histo[it->first]/(float)c->total; 
	}
}

float id_cluster::dotprod(const id_cluster &a) const
{
	if (total==0 || a.total==0) return 0;

	double dp=0;
	uumap::const_iterator it1 = histo.begin();
	uumap::const_iterator it2 = a.histo.begin();

	assert(it1!=histo.end() && it2!=a.histo.end());

	while (1) {
		if (it1->first < it2->first) {
			it1 = histo.lower_bound(it2->first);
			if (it1 == histo.end()) break;
		} else if (it2->first < it1->first) {
			it2 = a.histo.lower_bound(it1->first);
			if (it2 == a.histo.end()) break;
		} else {
			dp += (double)it1->second/(double)total * (double)it2->second/(double)a.total;
			++it1;
			if (it1 == histo.end()) break;
			++it2;
			if (it2 == a.histo.end()) break;
		}
	}
	return dp/(total*a.total);
	//return n_com/(n < n_a ? n : n_a);
}

unsigned id_cluster::add(unsigned id, int amount)
{
	if (amount == 0) return 0;
	uumap::iterator hbin = histo.lower_bound(id);

	int tot = (int)total + amount;
	assert(tot>=0);
	total = (unsigned)tot;

	if (hbin == histo.end() || hbin->first != id) {
		assert(amount>0);
		histo.insert(hbin, pair<unsigned,unsigned>(id,amount));
		return amount;
	} else {
		if ((int)hbin->second + amount <= 0) {
			// remove the entry for 'id'
			histo.erase(hbin);
			return 0;
		} else {
			hbin->second += amount;
			return hbin->second;
		}
	}
}

unsigned id_cluster::get_freq(unsigned id)
{
	id_cluster::uumap::iterator r = histo.find(id);
	if (r == histo.end())
		return 0;
	return r->second;
}

void id_cluster::print() const {
	for (id_cluster::uumap::const_iterator id_it(histo.begin()); id_it != histo.end(); ++id_it) {
		cout << "\t" <<  id_it->first <<":" << id_it->second 
			<< "(" << (float)id_it->second/(float)total << ")\n";
	}
}

void id_cluster_collection::print()
{
	unsigned non_ambig=0;
	cout << "Ambiguities:\n";
	for (id2cluster_map::iterator it(id2cluster.begin()); it!=id2cluster.end(); ++it)
	{
		int n=0;
		for (cluster_map::iterator cit=it->second.begin(); cit!=it->second.end(); ++cit) {
			float p = cit->second;
			if (p>.1) n++;
		}
		if (it->second.size()>0) {
			cout << " " << it->second.size() << "("<<n<<") ";
			if (it->second.size()==1) non_ambig++;
		}
	}
	cout << "\n.. and " << non_ambig << " non-ambiguous nodes, over " << id2cluster.size() 
		<< ". Total number of clusters: " << clusters.size()
		<< endl;
}

bool id_cluster_collection::save(const char *fn)
{
	FILE *f = fopen(fn,"wb");
	if (!f) {
		perror(fn);
		return false;
	}
	bool r= save(f);
	fclose(f);
	return r;
}

bool id_cluster_collection::save(FILE *f)
{
	unsigned m1 = 0xFFFFFFFF;
	bool err=false;
	int n=0;
	for (cluster_set::iterator it=clusters.begin(); !err && it!=clusters.end(); ++it) {
		if (fwrite(&m1, sizeof(unsigned), 1, f) != 1) { err=true; break; }
		n++;
		//if ((*it)->histo.size()==0) cerr << fn << ": writing an empty cluster!\n";
		
		for (id_cluster::uumap::iterator id_it((*it)->histo.begin()); id_it != (*it)->histo.end(); ++id_it) 
		{
			if (fwrite(&id_it->first, sizeof(unsigned), 1, f)!=1) { err=true; break; }
			if (fwrite(&id_it->second, sizeof(unsigned), 1, f)!=1) { err=true; break; }
		}
	}
	//cout << fn << ": wrote " << n << " clusters\n";
	return !err;
}

bool id_cluster_collection::save(sqlite3 *db, const char *tablename)
{
	if (db==0) return false;
	int rc = sqlite3_exec(db,"begin",0,0,0);
	assert(rc==0);

	const char *table = (tablename ? tablename : "clusters");
	char q[1024];
	sprintf(q, "drop table if exists %s;\n"
		"create table %s ("
		"cid_val integer primary key,"
		"cnt integer);\n", table, table);
	char *errmsg=0;
	rc=sqlite3_exec(db,q,0,0,&errmsg);
	if (rc) {
		cerr << table << ": can't create/clear table: rc=" << rc << endl << sqlite3_errmsg(db) << endl;
		cerr <<"Query: " << q << endl;
		if (errmsg) cerr << errmsg << endl;
		sqlite3_exec(db,"rollback",0,0,0);
		return false;
	}

	sqlite3_stmt *insert;
	sprintf(q,"insert into %s (cid_val, cnt) values (?,?)", table);
	rc=sqlite3_prepare_v2(db, q, -1, &insert, 0);
	assert(rc == 0);

	for (cluster_set::iterator it=clusters.begin(); it!=clusters.end(); ++it) {
		
		for (id_cluster::uumap::iterator id_it((*it)->histo.begin()); id_it != (*it)->histo.end(); ++id_it) 
		{
			sqlite3_int64 cid_val = (*it)->id;
			cid_val = (cid_val << 32) | id_it->first;
			sqlite3_bind_int64(insert, 1, cid_val);
			sqlite3_bind_int(insert, 2, id_it->second);
			if (sqlite3_step(insert) != SQLITE_DONE) {
				cerr << "error while inserting rows: " << sqlite3_errmsg(db)<< endl;
				sqlite3_free(errmsg);
				sqlite3_finalize(insert);
				sqlite3_exec(db,"rollback",0,0,0);
				return false;
			}
			sqlite3_reset(insert);
		}
	}
	sqlite3_exec(db,"commit",0,0,0);
	sqlite3_finalize(insert);
	//cout << fn << ": wrote " << n << " clusters\n";
	return true;
}

bool id_cluster_collection::load(sqlite3 *db, const char *tablename)
{
	if (db==0) return false;
	const char *table = (tablename ? tablename : "clusters");
	char q[1024];
	sprintf(q, "select cid_val,cnt from %s", table);
	sqlite3_stmt *select;
	int rc =sqlite3_prepare_v2(db, q, -1, &select, 0);
	if (rc) {
		cerr << "DB error: " << sqlite3_errmsg(db)<< endl;
		return false;
	}

	std::vector<id_cluster *> clusters;
	while (sqlite3_step(select) == SQLITE_ROW) {
		sqlite3_int64 cid_val = sqlite3_column_int64(select, 0);
		unsigned cid = cid_val >> 32;
		unsigned val = cid_val & 0xffffffff;
		unsigned cnt = sqlite3_column_int(select, 1);

		assert(cnt!=0);

		if (cid >= clusters.size()) {
			clusters.insert(clusters.end(), cid+1-clusters.size(), 0);
		}
		id_cluster *c;
		if (clusters[cid] == 0) 
			c = clusters[cid] = new id_cluster;
		else
			c = clusters[cid];

		c->add(val,cnt);
	}
	sqlite3_finalize(select);

	for (int i=1; i<clusters.size(); i++) {
		assert(clusters[i]);
		assert(clusters[i]->total>0);
		add_cluster(clusters[i]);
	}

	is_idf_normalized = false;
	cmp_best_clusters();
	return true;
}

bool id_cluster_collection::load(const char *fn)
{
	FILE *f = fopen(fn,"rb");
	if (!f) {
		perror(fn);
		return false;
	}
	version++;

	id_cluster *c = 0;

	unsigned val;
	bool err=false;
	int n=0;
	while (fread(&val, sizeof(unsigned), 1, f)==1) {
		if (val==0xFFFFFFFF) {
			if (c) {
				if (c->total==0) { err=true; break; }
				n++;
			       add_cluster(c);
			}
			c = new id_cluster();
		} else {
			if (!c) { err=true; break; }
			unsigned cnt;
			if (fread(&cnt, sizeof(unsigned), 1, f) != 1) { err=true; break; }
			if (cnt==0) {err=true; break;}
			c->add(val,cnt);
		}
	}

	if (!err) {
		if (c->total==0) err=true;
		else {
			n++;
			add_cluster(c);
		}
	}
	if (err) {
		cerr << fn << ": file format error\n";
		fclose(f);
		return false;
	}
	
	//cout << fn << ": read " << n << " clusters.\n";
	fclose(f);
	is_idf_normalized = false;
	cmp_best_clusters();
	return true;
}

id_cluster_collection::~id_cluster_collection()
{
	for (cluster_set::iterator it=clusters.begin(); it!=clusters.end(); ++it) 
		delete *it;
}

void id_cluster_collection::cmp_best_clusters()
{
	best_cluster.clear();
	for (id2cluster_map::iterator it(id2cluster.begin()); it!=id2cluster.end(); ++it)
	{
		float best_p=0;
		id_cluster *best_c=0;

		for (cluster_map::iterator cit=it->second.begin(); cit!=it->second.end(); ++cit) {
			float p = cit->second;
			//float p = (*cit)->histo[it->first];
			if (p>best_p) {
				best_p=p;
				best_c=cit->first;
			}
		}
		best_cluster[it->first] = best_c;
	}
}

unsigned id_cluster_collection::get_best_cluster(unsigned id)
{
	map<unsigned, id_cluster *>::iterator it = best_cluster.find(id);
	if (it != best_cluster.end()) return it->second->id;
	return 0;
}

// perform aglomerative clustering
void id_cluster_collection::reduce(float threshold)
{
	/*
	for (id2cluster_map::iterator it(id2cluster.begin()); it!=id2cluster.end(); ++it) {
		for (cluster_map::iterator j(it->second.begin()); j!=it->second.end(); ++j) {
			j->second = j->first->histo[it->first] / (float) j->first->total; 
		}
	}
	*/

	cout << "Building initial distance matrix...\n";
	build_distance_matrix(threshold);
	cout << "\ndone. ";

	unsigned start = clusters.size();
	unsigned merged=0;
	cout << "Starting clustering...\n";
	do {
		// find the two closest clusters
		set<cluster_dist_t>::reverse_iterator closest_it = distance_matrix.rbegin();

		if (closest_it == distance_matrix.rend()) {
			cout << "Empty distance matrix.\n";
			break;
		}
		cluster_dist_t closest = *closest_it;

		// if the are too far away, stop here.
		if (closest.d < threshold) {
			cout << "highest dotprod is " << closest.d <<  ", stopping clustering.\n";
			break;
		}
		merged++;
		//cout << "Merging clusters " << closest.a->id << " and " <<
		//	closest.b->id << " with distance " << closest.d << endl;

		// otherwise merge clusters and update distance matrix
		remove_from_distance_matrix(closest.a);
		remove_from_distance_matrix(closest.b);

		//cout << "Merging clusters.." << flush;
		merge_clusters(closest.a, closest.b);
		//cout << "done.\n";
		add_to_distance_matrix(closest.a, threshold);
	} while(1);
	cout << merged << " merges. " << clusters.size() << " clusters left, over " << start << endl;
	version++;

	int i=0;
	for (cluster_set::iterator it(clusters.begin()); it!=clusters.end(); ++it)
		(*it)->id = ++i;
}

void id_cluster_collection::remove_cluster(id_cluster *c)
{
	cluster_set::iterator it = clusters.find(c);
	if (it != clusters.end()) {
		clusters.erase(it);

		// update the id2cluster map
		for (id_cluster::uumap::iterator i = c->histo.begin(); i!=c->histo.end(); ++i) {
			id2cluster_map::iterator id2c( id2cluster.find(i->first) );

			id2c->second.erase(c);
			if (id2c->second.size() == 0)
				id2cluster.erase(id2c);
		}
	}
	version++;
}

void id_cluster_collection::update_cluster(id_cluster *c, unsigned id, int amount)
{
	if (amount==0) return;

	int val = c->add(id, amount);

	if ( flags & (QUERY_BIN_FREQ | QUERY_IDF)) is_idf_normalized=false;
	else c->weighted_sum = c->total;
	version++;

	if (clusters.find(c) != clusters.end()) {
		// the cluster is indexed. Let's update the id2cluster table.

		if (val==0) {
			// remove entry for 'id'
			id2cluster_map::iterator id2c( id2cluster.find(id) );
			if (id2c != id2cluster.end()) {
				// this can be heavy for vecmaps!
				id2c->second.erase(c);
			}
		} else {
			// update the entry for 'id'
			// this can be heavy for vecmaps!
			id2cluster[id][c]=val;
		}
	}
}

void id_cluster_collection::build_distance_matrix(float t)
{
	distance_matrix.clear();
	int n=0;
	float tot = clusters.size();
	for (cluster_set::iterator i=clusters.begin(); i!=clusters.end(); ++i) {
		add_to_distance_matrix(*i, t);
		printf("% 3d%% - % 8d elements\r", (int)(100.0f* (++n)/tot), (int)distance_matrix.size());
		fflush(stdout);
	}
}

void id_cluster_collection::remove_from_distance_matrix(id_cluster *c)
{
	cluster_score_map similar;
	get_scores(c, similar);
	//cout << "Removing " << similar.size() << " entries in dist matrix for " << c->id << flush ;
	for (cluster_score_map::iterator i=similar.begin(); i!=similar.end(); ++i) {
		cluster_dist_t cd(i->first, c, i->second);
		distance_matrix.erase(cd);
	}
	
	/*
	//cout << "done. Second check:\n";
	for (set<cluster_dist_t>::iterator it=distance_matrix.begin(); it!=distance_matrix.end(); ++it)
	{
		if (it->a == c || it->b == c) {
			cout << "bug in remove_from_distance_matrix. Buggy element:"
				<< " d="<<it->d << " a=" << it->a->id 
				<< " b=" << it->b->id 
				<< ", while removing " << c->id << endl;
			cluster_dist_t cd1(it->a, it->b, it->a->dotprod(*(it->b)));
			cluster_dist_t cd2(it->b, it->a, it->b->dotprod(*(it->a)));
			assert(!(cd1<cd2) && !(cd2<cd1));

			cout << "Cluster a: " << it->a->id << ":\n";
			it->a->print();
			cout << "Cluster b: " << it->b->id << ":\n";
			it->b->print();

			cout << "*it < cd1: " << ( *it < cd1 ? "yes" : "no" ) << endl;
			cout << "cd1 < *it: " << ( cd1 < *it ? "yes" : "no" ) << endl;

			cout << "Searching dist mat for: " 
				<< " d="<<cd1.d << " a=" << cd1.a->id 
				<< " b=" << cd1.b->id << endl;
			set<cluster_dist_t>::iterator entry1 = distance_matrix.find(cd1);
			assert (entry1 != distance_matrix.end());
			set<cluster_dist_t>::iterator entry2 = distance_matrix.find(cd2);
			assert (entry2 != distance_matrix.end());
			assert (entry1->a == entry2->a);
			assert(false);
		}
	}
	*/
	
}

void id_cluster_collection::add_to_distance_matrix(id_cluster *c, float threshold)
{
	cluster_score_map similar;
	get_scores(c, similar);
	//cout << "Adding " << similar.size() << " entries in dist matrix.. " << flush;
	for (cluster_score_map::iterator i=similar.begin(); i!=similar.end(); ++i) {
		cluster_dist_t cd(i->first, c, i->second);
		if (cd.d > threshold)
			distance_matrix.insert(cd);
	}
	//cout << "done.\n";
}


id_cluster_collection::cluster_dist_t::cluster_dist_t(id_cluster *_a, id_cluster *_b, float _d) 
{
	assert(_a!=_b);
	if (_a < _b) {
		a = _a;
		b = _b;
	} else {
		a=_b;
		b=_a;
	}
	//d = a->dotprod(*b);
	d = _d;
}

bool id_cluster_collection::cluster_dist_t::operator < (const cluster_dist_t &c) const
{
	if (d == c.d) {
	//if (fabs(d-c.d) < 1e-7) {
		if (a < c.a) return true;
		else {
			if (a==c.a) 
				return b < c.b;
			else 
				return false;
		}
	} 
	if (d<c.d) return true;
	return false;
}

void id_cluster_collection::clear()
{
	for (cluster_set::iterator it(clusters.begin()); it!=clusters.end(); it++) 
		delete *it;
	clusters.clear();
	id2cluster.clear();
	best_cluster.clear();
	distance_matrix.clear();
	version++;
}

float id_cluster_collection::idf(id_cluster_collection::id2cluster_map::iterator &id_it)
{
	if (clusters.size()<2) return 1;
	return logf(clusters.size()/id_it->second.size());
}

float id_cluster_collection::idf(unsigned id)
{
	id2cluster_map::iterator id_it = id2cluster.find(id);
	if (id_it == id2cluster.end()) return 0;
	return idf(id_it);
}

incremental_query::incremental_query(id_cluster_collection *db) 
	: database(db)
{
	if (db) version=db->version;
}

void incremental_query::clear() {
	results.clear();
	query_cluster.clear();
	scores.clear();
	if (database) version=database->version;
}

//template <typename T> T min(const T a, const T b) { return (a<b?a:b); }

void incremental_query::modify(unsigned id, int amount)
{
	if (!database) return;
	if (amount ==0) return;

	//float t_weight = database.weight(id);

	// update the query histogram
	unsigned amount_to_add = query_cluster.add(id,amount);
	unsigned amount_to_remove = amount_to_add - amount;

	id_cluster_collection::id2cluster_map::iterator id_it = database->id2cluster.find(id);
	if (id_it==database->id2cluster.end()) 
		return;
	float idf =  1;
	
	if (database->flags & id_cluster_collection::QUERY_IDF)
		idf = database->idf(id_it);

	database->set_query_rules(database->flags);


	for (id_cluster_collection::cluster_map::iterator cit=id_it->second.begin(); 
			cit!=id_it->second.end(); ++cit) 
	{
		float idf_n = idf;

		if (database->flags & id_cluster_collection::QUERY_NORMALIZED_FREQ) 
			idf_n = idf/cit->first->weighted_sum;

		float score_to_add;
		float score_to_remove;

		float tf = cit->second;
		if (database->flags & id_cluster_collection::QUERY_BIN_FREQ) {
			amount_to_remove = min(amount_to_remove, (unsigned)1);
			amount_to_add = min(amount_to_add, (unsigned)1);
			tf = min(tf,1.0f);
		}

		if (database->flags & id_cluster_collection::QUERY_MIN_FREQ) {
			score_to_remove = min(tf, (float)amount_to_remove) * idf_n;
			score_to_add = min(tf, (float)amount_to_add) * idf_n;
		} else {
			score_to_add = amount_to_add * tf * idf_n;
			score_to_remove = amount_to_remove * tf * idf_n;
		}

		cluster_score_map::iterator __i = scores.lower_bound(cit->first);

		float s;
		if (__i == scores.end() || cit->first<(*__i).first) {
			// the dot product does not contain an entry for cit->first
			s = score_to_add;
			if (s>0) scores.insert(__i, pair<id_cluster *, double>(cit->first, s));
			//assert(amount_to_remove==0);
		} else {
			s = __i->second + score_to_add - score_to_remove;
			__i->second= s;
		}
	}
}

incremental_query::iterator incremental_query::sort_results(unsigned max_results)
{
	if (!database) return end();
	results.clear();
	for (cluster_score_map::iterator it(scores.begin()); it!=scores.end(); ++it)
	{
		float s = it->second;
	        if (database->flags & id_cluster_collection::QUERY_NORMALIZED_FREQ) {
			if ((database->flags & (id_cluster_collection::QUERY_MIN_FREQ | id_cluster_collection::QUERY_BIN_FREQ)) ==0)
				s = s/(float)query_cluster.total;
		}

		if (results.size() < max_results) {
			results.insert(ranked_cluster(it->first, s));
		} else if (results.rbegin()->score < s) {
			std::set< ranked_cluster >::iterator last(results.end());
			--last;
			results.erase(last);
			results.insert(ranked_cluster(it->first, s));
		}
	}

	/*
	for (set<ranked_cluster>::const_iterator it(results.begin()); it!=results.end(); ++it)
	{
		cout << "Entry " << it->c << " has score: " << it->score << endl;
	}
	*/
	return begin();
}

incremental_query::iterator incremental_query::sort_results_min_ratio(float ratio)
{
	if (!database) return end();
	results.clear();
	for (cluster_score_map::iterator it(scores.begin()); it!=scores.end(); ++it)
	{
		float s = it->second;
	        if (database->flags & id_cluster_collection::QUERY_NORMALIZED_FREQ) {
			if ((database->flags & (id_cluster_collection::QUERY_MIN_FREQ | id_cluster_collection::QUERY_BIN_FREQ)) ==0)
				s = s/(float)query_cluster.total;
		}

		iterator first = begin();
		if (first == end()) {
			results.insert(ranked_cluster(it->first, s));
		} else if (first->score*ratio < s) {
			results.insert(ranked_cluster(it->first, s));
		}
	}

	iterator first = begin();
	if (first != end()) {
		ranked_cluster limit(0,first->score * ratio);
		results.erase(lower_bound(begin(),end(), limit), end());
	}
	return begin();
}

void incremental_query::set(id_cluster *q)
{
	if (!database) return;
	id_cluster::uumap::iterator goal(q->histo.begin());
	id_cluster::uumap::iterator actual(query_cluster.histo.begin());


	if (version!= database->version) {
		// DB changed. Reset the query.
		clear();
		for (goal = q->histo.begin(); goal != q->histo.end(); ++goal)
			modify(goal->first, goal->second);
		return;
	}

	unsigned modifs=0;
	unsigned q_size = q->histo.size();
	vecmap<unsigned,int> modif_map;
	modif_map.reserve(q_size+ query_cluster.histo.size());

	while (goal!=q->histo.end() && actual!=query_cluster.histo.end()) {

		if (actual->first == goal->first) {
			if (goal->second != actual->second) {
				modif_map[actual->first] = goal->second - actual->second;
			}
			++actual;
			++goal;
		} else if (actual->first < goal->first) {
			modif_map[actual->first] = -actual->second;
			++actual;
		} else {
			modif_map[goal->first] = goal->second;
			++goal;
		}
	}
	while (goal != q->histo.end()) {
		modif_map[goal->first] = goal->second;
		++goal;
	}
	while (actual != query_cluster.histo.end()) {
		modif_map[actual->first] = -actual->second;
		++actual;
	}

	if (modifs>= q_size) {
		// too many modification. Reset the query.
		clear();
		for (goal = q->histo.begin(); goal != q->histo.end(); ++goal)
			modify(goal->first, goal->second);
	} else {
		// update the query
		for (vecmap<unsigned,int>::iterator it(modif_map.begin()); it!=modif_map.end(); ++it)
			modify(it->first, it->second);
	}
}

id_cluster *incremental_query::get_best(float *score)
{
	if (!database) return 0;
	sort_results(5);
	std::set< ranked_cluster >::iterator it = results.begin();
	if (it!=results.end()) {
		if (score) *score = it->score;
		return it->c;
	}
	return 0;
}

