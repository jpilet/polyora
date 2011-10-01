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
#ifndef DB_ENTRY_H
#define DB_ENTRY_H

#include <vector>
#include <map>
#include <string>

#include "kpttracker.h"
#include "idcluster.h"
#include "sqlite3.h"

/*! \defgroup RetrievalGroup Visual objects retrieval
*/
/*@{*/

typedef sqlite3_int64 img_id;
class db_keypoint : public pyr_keypoint {
public:

	db_keypoint(unsigned cid, img_id img) : image(img) { this->cid = cid; }
	db_keypoint(const pyr_keypoint &a, img_id img) : pyr_keypoint(a), image(img) { }

	img_id image;

	bool operator < (const db_keypoint &a) const { return cid < a.cid; }
};

class visual_database;

/*! Entry for an object database.
 *
 * Each frame or group of frames representing an object is stored in the
 * database as a histogram of the cluster ids of its keypoints. The histogram
 * itself is in the parent class: id_cluster.
 */
class visual_object : public id_cluster {
public:

	//typedef std::vector<db_keypoint> db_keypoint_vector;
	typedef std::multiset<db_keypoint> db_keypoint_vector;

	visual_object(visual_database *, sqlite3_int64 id, const char *comment="", int flags=0);
	virtual ~visual_object();

	void prepare();
	void find(unsigned id, db_keypoint_vector::iterator &start, db_keypoint_vector::iterator &end);


	sqlite3_int64 id() const { return obj_id; }

	// returns a pointer to the newly added keypoint
	db_keypoint* add_keypoint(pyr_keypoint *p, img_id img);
	unsigned add_frame(pyr_frame *frame);
	void add(db_keypoint &p);

	unsigned get_total() const { return total; }

	struct correspondence {
		db_keypoint *obj_kpt;
		pyr_keypoint *frame_kpt;
		float correl;
		correspondence(db_keypoint *o, pyr_keypoint *f, float c) : obj_kpt(o), frame_kpt(f), correl(c) {}
		bool operator<(const correspondence &a) const { return correl > a.correl; }
	};
	typedef std::vector<correspondence> correspondence_vector;
	float get_correspondences(pyr_frame *frame, correspondence_vector &corresp);
	float get_correspondences_std(pyr_frame *frame, correspondence_vector &corresp);
	float verify(pyr_frame *frame, correspondence_vector &corresp);

	const std::string &get_comment() const { return comment; }

	enum { VERIFY_HOMOGRAPHY=1,VERIFY_FMAT=2 };
	int get_flags() const { return flags; }
	unsigned long get_id() {return static_cast<unsigned long>(obj_id);}

	struct annotation {
		sqlite3_int64 id;
		float x, y;
		std::string descr;
		int type;

		annotation(sqlite3_int64 id, float x, float y, std::string descr, int type)
			: id(id), x(x), y(y), descr(descr), type(type) {}
	};
	typedef std::vector<annotation> annotations_vector;
	typedef annotations_vector::const_iterator annotation_iterator;
protected:
	annotations_vector annotations;
public:
	void add_annotation(float x, float y, int type, const std::string& text);
	unsigned get_annotations_count() const { return (unsigned)annotations.size(); }
	annotation_iterator annotation_begin() const { return annotations.begin(); }
	annotation_iterator annotation_end() const { return annotations.end(); }
	const annotation& get_annotation(unsigned id) const { return annotations.at(id); }

	// Either 0, or pointing to a 3x3 matrix for F-Mat or homography.
	CvMat *M;

protected:
	bool sorted;

	db_keypoint_vector points;

	sqlite3_int64 obj_id;

public:
	int nb_points() const { return points.size(); }

	visual_database *vdb;
	img_id representative_image;
protected:
	std::string comment;
	int flags;

	friend class visual_database;
};

/*! A database of visual objects.
 *
 * The database must 
 */
class visual_database : public id_cluster_collection {
public:

	visual_database(id_cluster_collection::query_flags flags );
	virtual ~visual_database();

	/*! Reads or create a sqlite3 database file for visual objects.
	 * Calling this method is required. If the file does not exists, it is created.
	 * If the file exists, the method indexes every entry in it. After a
	 * successfull call to open(), query_frame or create_incremental_query()
	 * are ready to be called.
	 */
	bool open(const char *fn);

	/*! Connects to an already opened sqlite3 database.
	 * this method can be called instead of open().
	 */
	bool connect_to_db(sqlite3 *sql3db);

	/*! Creates a new %visual_object. 
	 * The object is in the database, but not in the index. No query will
	 * find it until add_to_index() is called.
	 */
	visual_object *create_object(const char *comment=0, int flags=0);
	void add_to_index(visual_object *o) { add_cluster(o); }

	bool remove_object(visual_object *o);

	/*! Stores an image in the database. */
	img_id add_image(IplImage *im);

	/*! Fetches an image from the database.
	 * It also caches the image, so that subsequent fetching of the same
	 * image is very fast.
	 */
	IplImage *get_image(img_id im_id);

	visual_object *query_frame(pyr_frame *frame, float *score=0);

	incremental_query *create_incremental_query() { return new incremental_query(this); }

	void start_update() { exec_sql("begin"); }
	void finish_update() { exec_sql("commit"); }

	sqlite3 *get_sqlite3_db() { return db; }
protected:
	sqlite3 *db;

	sqlite3_stmt *get_cached_stmt(const char *query, bool verbose=true);
	bool exec_sql(const char *query);

	typedef std::map<img_id, IplImage *> image_cache_map;
	image_cache_map image_cache;

	typedef std::map<const char *, sqlite3_stmt *> stmt_cache_map;
	stmt_cache_map stmt_cache;

	friend class visual_object;
};


void update_query_with_frame(incremental_query &query, kpt_tracker *tracker);
void init_query_with_frame(incremental_query &query, pyr_frame *frame);

/* SQLite database:
 *
 
  create table images (
  	img_id integer primary key autoincrement,
	width integer,
	height integer,
	step integer,
	data blob
  );

  create table Objects (
  	obj_id integer primary key autoincrement,
	timestamp char(19) default CURRENT_TIMESTAMP,
	comment text
  );

  create table Keypoints ( 
  	kpt_id integer primary key autoincrement,
  	obj_id integer,
  	cid integer,
	img_id integer,
  	u real,
  	v real,
  	scale real,
	orient real
  );

 
 * To create the 'Reverse' table:
 
  create table Reverse as 

  select cid, obj_id, COUNT(kpt_id) as weight
  from keypoints 
  group by cid,obj_id;

  create index Reverse_index on Reverse (cid, obj_id);

 * Selecting all images of an object
 	select img_id
	from Keypoints
	where obj_id = 'id'
	group by img_id

 */

/*@}*/
#endif
