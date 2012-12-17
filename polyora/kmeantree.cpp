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
#ifdef WIN32
#define _CRT_SECURE_NO_DEPRECATE
#endif

#include <math.h>
#include <assert.h>
#include "kmeantree.h"
#ifdef NDEBUG
#undef NDEBUG
#endif

#include <string.h>
#include <iostream>
#include <math.h>
#include <stdlib.h>
#include <map>
#include <unistd.h>
#ifdef _OPENMP
#include <omp.h>
#endif

using namespace kmean_tree;
using namespace std;

#ifdef WIN32
#include <float.h>
static inline double drand48() {
	return (double)rand()/(double)RAND_MAX;
}

static inline int finite(float f) {
	return _finite(f);
}
#endif

mean_t::mean_t() {
	memset(mean,0,descriptor_size*sizeof(float));
}

node_t::node_t() {
	for (unsigned i=0; i<nb_branches; i++)
		clusters[i] = 0;
}

#ifdef _OPENMP
omp_lock_t lock;
int thread_count=1;
#endif

void node_t::recursive_split(int max_level, int min_elem, int level) {
	if (level >= max_level) {
		std::cout << "stopping splitting at depth " << level << ", with " 
			<< data.size() << " elements\n";
		return;
	}

	if (is_leaf()) {
		if (data.size()>(unsigned)min_elem) {
			run_and_split();
		} else {
			std::cout << "stopping splitting at depth " << level << ", with "
				<< data.size() << " elements\n";
		}
	}
#ifdef _OPENMP
	omp_set_lock(&lock);
	int thread_add=0;
	if(thread_count<8) {
		omp_set_nested(1);
		thread_add=3;
		thread_count+=thread_add;
	} else { 
		omp_set_nested(0);
	}
	omp_unset_lock(&lock);
#pragma omp parallel for
#endif
	for (int i=0; (unsigned)i<nb_branches; i++)
		if (clusters[i])
			clusters[i]->recursive_split(max_level, min_elem, level+1);
#ifdef _OPENMP
	omp_set_lock(&lock);
	thread_count-=thread_add;
	omp_unset_lock(&lock);
#endif
}

unsigned node_t::get_id(descriptor_t *descr, node_t **node, int depth)
{
	if (is_leaf()) {
		if (node) *node = this;
		return id;
	}

	// find nearest cluster
	std::multimap<double,int> scores;
	cmp_scores(scores, descr);
	std::multimap<double,int>::iterator it = scores.begin();

	return clusters[it->second]->get_id(descr, node, depth+1);
}

bool node_t::run_and_split() {
	assert(is_leaf());

	// allocate branches
	for (unsigned i=0; i<nb_branches; i++) {
		clusters[i] = new node_t;
	}

	run_kmean(); 

	// forget data
	data.clear();
	return true;
}

void node_t::assign_leaf_ids(unsigned *id_ptr)
{
	unsigned *ptr=id_ptr;
	if (id_ptr==0) {
		ptr=&id;
	}
	id=0;
	if (is_leaf())
		id = ++(*ptr);
	else {
		for (unsigned i=0; i<nb_branches; i++)
			if (clusters[i])
				clusters[i]->assign_leaf_ids(ptr);
	}
}

void node_t::cmp_scores(std::multimap<double,int> &scores, descriptor_t *data)
{
	scores.clear();
	for (unsigned i=0; i<nb_branches; i++) {
		if (clusters[i]) {
			double d = clusters[i]->mean.distance(data);
			//if (!finite(d)) std::cout << "dist= " << d << std::endl;
			scores.insert(std::pair<double,int>(d,i));
		}
	}
}


void node_t::run_kmean(int nb_iter) 
{
	bool online_kmean = false;
	int n = data.size();
	int k = nb_branches;

	assert(n>k);
	assert(k>=2);

	std::vector<float> counter(k, 0);
	std::vector<mean_t> new_mean(k);

	std::cout << "(starting k-mean:" ;
	std::cout << " total: "<< n << ")" << std::endl;

	// initialization: randomly pick some data to initialize the centers
	for (int i=0; i<k; i++) {
				
		int nb_init=1;
		unsigned d = drand48()*n;
		clusters[i]->mean.accumulate(0, 1.0f/nb_init, data[d]);
		for (int k=1; k<nb_init; k++) {
			d = drand48()*n;
			clusters[i]->mean.accumulate(1, 1.0f/nb_init, data[d]);
		}
	}

	for (int iter=0;iter<nb_iter;iter++) {

		// reset counters
		for (int i=0; i<k; i++) {
			counter[i]=0;
		}

		// process data
		for (int j=0; j<n; j++) {

			// find nearest cluster
			std::multimap<double,int> scores;
			cmp_scores(scores, data[j]);

			std::multimap<double,int>::iterator b = scores.begin();
			std::multimap<double,int>::iterator b2 = b;
			++b2;

			int best_m = b->second;

			if (iter==nb_iter-1) {
				// last iteration, save cluster attribution
				clusters[best_m]->data.push_back(data[j]);
				if (0 && b2->first/b->first > .98) {
					// too close to the border!
					clusters[b2->second]->data.push_back(data[j]);
				}
			}

			// update center
			counter[best_m] += 1;
			if (online_kmean) {
				clusters[best_m]->mean.accumulate(.95, .05, data[j]);
			} else {
				new_mean[best_m].accumulate(
						(counter[best_m]-1.0f)/counter[best_m], 
						1.0f/counter[best_m], 
						data[j]);
			}
		}

		/*
		   for (int i=0; i<k; i++) {
		   	std::cout << "C" << i << ":" << counter[i] << ":";
		   	for (int j=0; j<16; j++) 
		   		std::cout << " " << clusters[i]->mean.mean[j];
		   	std::cout << std::endl;
		   }
		 */

		if (!online_kmean) 
			for (int i=0; i<k; i++) {
				for (unsigned j=0; j<descriptor_size; j++) 
					clusters[i]->mean.mean[j] = new_mean[i].mean[j];
			}
	}
	for (int i=0; i<k; i++) {
		if (counter[i]==0) {
			if (clusters[i]->data.size()>0) {
				std::cout << "Warning, killing a cluster with " << clusters[i]->data.size()
					<< " elements in it!\n";
			}
			delete clusters[i];
			clusters[i]=0;
		}
		else {
			std::cout << "  C" << i << ": " << clusters[i]->data.size();
			//for (int j=0; j<16; j++) std::cout << " " << clusters[i].mean.mean[j];
		}
	}
	std::cout << std::endl;
}

void mean_t::accumulate(float a, float b, descriptor_t *d)
{
	if (a==0) {
		for (unsigned i=0; i<descriptor_size; i++) {
			mean[i] = b*d->descriptor[i];
			//assert(finite(mean[i]));
		}
	} else {
		for (unsigned i=0; i<descriptor_size; i++) {
			mean[i] = mean[i]*a + b*d->descriptor[i];
			//assert(finite(mean[i]));
		}
	}

	/*
	if (sum<=0)
			std::cout << "sum==0!\n";
	else {
	sum = 1.0f/sum;
	for (int i=0; i<64; i++) {
		mean[i] *= sum;
	}
	}
	*/
}

double mean_t::distance(descriptor_t *d) {
	float dist=0;
	for (unsigned i=0; i<descriptor_size; i++) {
		assert(finite(mean[i]));
		assert(finite(d->descriptor[i]));
		//dist -= mean[i]*d->descriptor[i];
		float di = mean[i]-(float)d->descriptor[i];
		dist += di*di;
                assert(finite(dist));
	}
	assert(finite(dist));
	return dist;
}

bool node_t::save(const char *filename) 
{
	/*
	autoserial::BinaryFileWriter bfw(filename);
	node_t *p = this;
	bfw.write(p);

	// check..
	node_t *tree;
	autoserial::BinaryFileReader bfr(filename);
	bfr.read((ISerializable **)&tree);
	tree->print_summary();
	*/
	FILE *f = fopen(filename,"wb");
	if (!f) {
		perror(filename);
		return false;
	}
	save(f);
	fclose(f);
	return true;
}

bool node_t::save_to_database(const char *fn)
{
	char *errmsg=0;
	sqlite3 *sql3;
	int rc = sqlite3_open(fn, &sql3);
	if (rc) {
		cerr << "Can't open database: " <<  sqlite3_errmsg(sql3) << endl;
		sqlite3_close(sql3);
		return false;
	}

	// create empty tables
	rc=sqlite3_exec(sql3,
			"create table if not exists tree_nodes ("
			"ptr integer primary key,"
			"id integer,"
			"mean blob);\n"
			"create table if not exists tree_structure ("
			"parent integer,"
			"child integer);\n"
			"delete from tree_nodes;\n"
			"delete from tree_structure;",
			0,0, &errmsg);
	if (rc) {
		cerr << fn << ": can't prepare tables: " <<  errmsg << endl;
		sqlite3_free(errmsg);
		sqlite3_close(sql3);
		return false;
	}

	sqlite3_stmt *insert_node=0;
	sqlite3_stmt *insert_child=0;
	rc = sqlite3_prepare_v2(sql3, "insert into tree_nodes (ptr,id,mean) values (?,?,?)", -1, &insert_node, 0);
	assert(rc==0);
	rc = sqlite3_prepare_v2(sql3, "insert into tree_structure (parent,child) values (?,?)", -1, &insert_child, 0);
	assert(rc==0);
	
	sqlite3_exec(sql3,"begin",0,0,0);
	if (save(sql3, insert_node, insert_child)) {
		sqlite3_bind_int64(insert_child, 1, 0);
		sqlite3_bind_int64(insert_child, 2, (sqlite3_int64) this);
		sqlite3_step(insert_child);

		sqlite3_exec(sql3,"commit",0,0,0);
	} else{ 
		sqlite3_exec(sql3,"rollback",0,0,0);
	}
	sqlite3_finalize(insert_node);
	sqlite3_finalize(insert_child);

	sqlite3_close(sql3);
	return true;
}


node_t *kmean_tree::load(sqlite3 *db)
{
	if (db==0) return 0;

	std::map<sqlite3_int64,node_t *> nodes;
	std::map<sqlite3_int64,node_t *>::iterator it;

	char *errmsg=0;

	// load all nodes
	const char *query="select ptr,id,mean from tree_nodes";
	sqlite3_stmt *stmt=0;
	const char *tail=0;
	int rc = sqlite3_prepare_v2(db, query, -1, &stmt, &tail);

	if (rc != SQLITE_OK) {
		cerr << "Error: " << sqlite3_errmsg(db) << endl;
		cerr << "While compiling: " << query << endl;
		return 0;
	}

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		sqlite3_int64 ptr = sqlite3_column_int64(stmt, 0);
		node_t *node = new node_t;
		node->id = sqlite3_column_int(stmt, 1);
		nodes[ptr] = node;
		int sz = sqlite3_column_bytes(stmt,2);
		assert(sz == sizeof(node->mean));
		memcpy(&node->mean, sqlite3_column_blob(stmt, 2), sizeof(node->mean));
	}
	sqlite3_finalize(stmt);

	query = "select parent,child from tree_structure";
	rc = sqlite3_prepare_v2(db, query, -1, &stmt, &tail);
	if (rc != SQLITE_OK) {
		cerr << "Error: " << sqlite3_errmsg(db) << endl;
		cerr << "While compiling: " << query << endl;
		return 0;
	}

	// fix pointers
	node_t *root=0;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		sqlite3_int64 parent_id = sqlite3_column_int64(stmt, 0);
		sqlite3_int64 child_id = sqlite3_column_int64(stmt, 1);

		it = nodes.find(child_id);
		assert(it!=nodes.end());
		node_t *child = it->second;

		if (parent_id==0) {
			root = child;
		} else {
			it = nodes.find(parent_id);
			assert(it!=nodes.end());
			node_t *parent = it->second;
			bool written=false;
			for (int i=0; i<nb_branches; ++i) {
				if (parent->clusters[i] ==0) {
					parent->clusters[i] = child;
					written=true;
					break;
				}
			}
			assert(written);
		}
	}
	for (it = nodes.begin(); it!=nodes.end(); ++it) {
		if (it->second->id == 0 && it->second->clusters[0]==0) {
			cerr << "entry " << it->first << " has id=0 and no children!\n";
		}
	}

	sqlite3_finalize(stmt);
	assert(root!=0);
	return root;
}

node_t *kmean_tree::load(const char *filename)
{
	/*
	node_t *tree;
	autoserial::BinaryFileReader bfr(filename);
	bfr.read((autoserial::ISerializable **) &tree);
	return tree;
	*/
	FILE *f = fopen(filename,"rb");
	if (!f) {
		perror(filename);
		return 0;
	}
	node_t *tree = new node_t;
	if (!tree->load(f)) {
		std::cerr << "loading failed\n";
		delete tree;
		tree = 0;
	}
	fclose(f);
	return tree;
}

#include <sys/types.h>
#include <sys/stat.h>
#ifndef WIN32
#include <sys/mman.h> /* mmap() is defined in this header */
#endif
#include <fcntl.h>

#include <string>
using namespace std;
void save_node_images(string prefix, kmean_tree::node_t *node);

node_t *kmean_tree::build_from_data(const char *filename, int max_level, int min_elem, int stop)
{
#ifndef WIN32
	int fd = open(filename,O_RDONLY);
	if (fd<0) {
		perror(filename);
		return 0;
	}

	// find size of input file
	struct stat statbuf;
	if (fstat (fd,&statbuf) < 0) {
		perror(filename);
		close(fd);
		return 0;
	}

	descr_file_packet *data = (descr_file_packet *) mmap(0,statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (data==(descr_file_packet *)-1) {
		perror("mmap");
		close(fd);
		return 0;
	}
	int ndata = statbuf.st_size / sizeof(descr_file_packet);
#else
	FILE *file = fopen(filename, "rb");
	if (!file) return 0;

	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	int ndata = size/ sizeof(descr_file_packet);
	descr_file_packet *data = new descr_file_packet[ndata];
	fseek(file,0,SEEK_SET);
	if (fread(data, sizeof(descr_file_packet), ndata, file) != ndata) {
		cerr << filename << ": can not read data\n";
		fclose(file);
		return 0;
	}
	fclose(file);
#endif

	node_t *root = new node_t;
	

	if (stop>0 && stop<ndata) ndata=stop;

	root->data.reserve(ndata);
	bool ok=true;
	for (int i=0; i<ndata; i++) {
		for (unsigned j=0; j<descriptor_size; j++) {
			if (!finite(data[i].d.descriptor[j])) {
				std::cout << "descriptor " << i << " has a problem in coord " << j << std::endl;
				ok=false;
			}
		}
		root->data.push_back(&data[i].d);
	}
	if (!ok) return 0;

	std::cout << "Data loaded. Starting k-mean."<<std::endl;
#ifdef _OPENMP
	omp_init_lock(&lock);
#endif
	root->recursive_split(max_level, min_elem);

	unsigned n = 0;
	root->assign_leaf_ids(&n);
	cout << "Tree has " << n << " leafs.\n";

	//save_node_images(string("T"), root);

#ifdef WIN32
	delete[] data;
#else
	munmap(data,statbuf.st_size);
	close(fd);
#endif
	return root;

}


void node_t::print_summary(int depth) {

	if (is_leaf()) return;
	for (unsigned i=0; i<nb_branches; i++) {
		for (int j=0;j<depth;j++) std::cout<< ".";
		std::cout << "C" << i <<": ";
		for (int j=0; j<16; j++) std::cout << " " << clusters[i]->mean.mean[j];
		std::cout<<std::endl;
		if (clusters[i])
			clusters[i]->print_summary(depth+1);
	}
}

bool node_t::load(FILE *f)
{
	unsigned char check, child;
	if (fread(&check, 1, 1, f) != 1) return false;
	if (check!= 0xab) {
		std::cerr << "check failed\n";
		return false;
	}

	if ((fread(&id, sizeof(id), 1, f) != 1)
		|| (fread(&mean, sizeof(mean), 1, f) != 1)
		|| (fread(&child,1,1,f) != 1)) 
	{
		std::cerr << "error: can't read tree data!\n";
		return false;
	}


	if (child==0 && id==0) {
		std::cerr << "child has no id!\n";
		return false;
	}
	if (child>nb_branches) {
		std::cerr<< "error: too many branches!\n";
		return false;
	}
	for (unsigned i=0;i<child;i++) {
		clusters[i] = new node_t;
		if (!clusters[i]->load(f)) return false;
	}
	return true;
}

bool node_t::save(sqlite3 *db, sqlite3_stmt *insert_node, sqlite3_stmt *insert_child)
{
	sqlite3_bind_int64(insert_node, 1, (sqlite3_int64)this);
	sqlite3_bind_int(insert_node, 2, id);
	sqlite3_bind_blob(insert_node, 3, &mean, sizeof(mean),  SQLITE_STATIC);
	if(sqlite3_step(insert_node)!=SQLITE_DONE) {
		printf("%s:%d: error: %s\n", __FILE__, __LINE__, sqlite3_errmsg(db));
		return false;
	}
	sqlite3_reset(insert_node);
	for (unsigned i=0; i<nb_branches; i++) {
		if (clusters[i]) {
			sqlite3_bind_int64(insert_child, 1, (sqlite3_int64)this);
			sqlite3_bind_int64(insert_child, 2, (sqlite3_int64)clusters[i]);
			if(sqlite3_step(insert_child)!=SQLITE_DONE) {
				printf("%s:%d: error: %s\n", __FILE__, __LINE__, sqlite3_errmsg(db));
				return false;
			}
			sqlite3_reset(insert_child);
		}
	}
	for (unsigned i=0; i<nb_branches; i++) {
		if (clusters[i]) 
			if (!clusters[i]->save(db,insert_node,insert_child)) return false;
	}
	return true;
}

bool node_t::save(FILE *f)
{
	unsigned char check=0xab, child = 0;
	for (unsigned i=0;i<nb_branches;i++) if (clusters[i]) ++child;

	if (fwrite(&check,sizeof(check),1,f)!=1) return false;
	if (fwrite(&id,sizeof(id), 1, f) != 1) return false;
	if (fwrite(&mean,sizeof(mean), 1, f) != 1) return false;
	if (fwrite(&child,sizeof(child),1,f)!=1) return false;
	if (child == 0 && id==0) {
		std::cerr << "Warning: writing a tree file that contains a leaf with id=0!\n";
	}
	if (child) {
		for (unsigned i=0; i<nb_branches; i++) {
			if (clusters[i]) {
				if (!clusters[i]->save(f)) return false;
			}
		}
	}
	return true;
}



#include <highgui.h>
#include <sstream>
template < class T >
string ToString(const T &arg)
{
	ostringstream	out;

	out << arg;

	return(out.str());
}
void save_patch(const string &name, float *f)
{
	CvMat mat;
	cvInitMatHeader(&mat, 16, 16, CV_32FC1, f);
	double min,max;
	cvMinMaxLoc(&mat, &min, &max);
	CvMat *m2 = cvCreateMat(16,16,CV_8UC1);
	cvCvtScale(&mat, m2, 255/(max-min), -min*255/(max-min));
	cvSaveImage(name.c_str(), m2);
	cvReleaseMat(&m2);
	cout << name << ": " << min << ", " << max << endl;
}
	
void save_node_images(string prefix, kmean_tree::node_t *node)
{
	if (!node) return;

	for (unsigned i=0; i<3; i++) {
		if (i<node->data.size()) {
			save_patch(prefix + "D" + ToString(i) + ".png", node->data[i]->descriptor);
		}
	}

	string name = prefix + ".png";

	save_patch(name,node->mean.mean);

	for (unsigned i=0;i<kmean_tree::nb_branches;i++) {
		save_node_images(prefix + "_" + ToString(i), node->clusters[i]);
	}
}

