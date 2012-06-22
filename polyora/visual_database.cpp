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

#include "visual_database.h"
#include <opencv2\calib3d\calib3d.hpp>
#include <algorithm>
#include <iostream>

using namespace std;

visual_object::visual_object(visual_database *vdb, sqlite3_int64 id, const char *comment, int flags)
       	: sorted(false), obj_id(id), vdb(vdb), comment(comment), flags(flags)
{
	M=0;
}

visual_object::~visual_object() {
	if (M) cvReleaseMat(&M);
}

void visual_object::add(db_keypoint &p)
{
	points.insert(p);
	//points.push_back(p);
	//sorted=false;

	// add the point to the histogram
	id_cluster::add(p.cid);
}

void visual_object::prepare()
{
	/*
	if (!sorted) {
		sort(points.begin(), points.end());
		sorted = true;
	}
	*/
}

void visual_object::find(unsigned id, db_keypoint_vector::iterator &start, db_keypoint_vector::iterator &end)
{
	prepare();
	db_keypoint k(id,0);
	//start = lower_bound(points.begin(), points.end(), k);
	start = points.lower_bound(k);
	if (start == points.end()) {
		end = points.end();
		return;
	}
	end = points.upper_bound(k);
	//end = upper_bound(start, points.end(), k);
}


visual_object *query_frame(id_cluster_collection &db, pyr_frame *frame, float *score)
{
	id_cluster q;

	for (tracks::keypoint_frame_iterator it=frame->points.begin(); !it.end(); ++it) 
	{
		pyr_keypoint * p = (pyr_keypoint *)  it.elem();
		if (p->cid) q.add(p->cid);
		
	}

	visual_object *r = (visual_object *) db.get_best_cluster(&q, score);
	return r;
}


struct zint {
	int v;
	zint() : v(0) {}
};

void update_query_with_frame(incremental_query &query, kpt_tracker *tracker)
{


	tframe *frame =  tracker->frames;
	assert(frame!=0);
#if 0
	map<unsigned, zint> updates;
	if (frame==0) return;

	tframe *lf = frame->frames.prev;

	for (ttrack *t=tracker->trk.all_tracks; t!=0; t = t->track_node.next)
	{
		assert(t->keypoints!=0);
		pyr_keypoint *k = (pyr_keypoint *)t->keypoints;
		pyr_keypoint *prev_k=0;
		if (k->matches.prev) prev_k = (pyr_keypoint *) k->matches.prev;
		if ((k->frame != frame && k->cid)) // lost track
		{
			updates[k->cid].v--;
		}  
		else if (k->frame==frame && prev_k && prev_k->cid != k->cid) // cid changed
		{
			if (prev_k->cid)
				updates[prev_k->cid].v--;
			if (k->cid)
				updates[k->cid].v++;
		}
		else if (k->cid && k->frame==frame // valid keypoint on current frame
				&& prev_k==0) // unmatched or invalid on previous frame
		{
			updates[k->cid].v++;
		}
	}
	for (map<unsigned,zint>::iterator it(updates.begin()); it!=updates.end(); ++it)
		query.modify(it->first, it->second.v);
#else
	id_cluster q;

	for (tracks::keypoint_frame_iterator it=frame->points.begin(); !it.end(); ++it) 
	{
		pyr_keypoint * p = (pyr_keypoint *)  it.elem();

		if (tracker->id_clusters==0) {
			if (p->id) q.add(p->id);
		} else {
			pyr_track *track = (pyr_track *) p->track;
			if (track ==0 || p->cid==0) continue;

			for(incremental_query::iterator it(track->id_histo.begin()); it!=track->id_histo.end(); ++it)
				q.add(it->c->id);
		}
		
	}
	
	query.set(&q);
#endif

}

void init_query_with_frame(incremental_query &query, pyr_frame *frame)
{
	id_cluster q;
	kpt_tracker &tracker(*frame->tracker);

	for (tracks::keypoint_frame_iterator it=frame->points.begin(); !it.end(); ++it) 
	{
		pyr_keypoint * p = (pyr_keypoint *)  it.elem();
		if (tracker.id_clusters) {
			if (p->cid) q.add(p->cid);
		} else {
			if (p->id) q.add(p->id);
		}
		
	}

	query.version = query.database->get_version();

	query.clear();
	for (id_cluster::uumap::iterator it(q.histo.begin()); it!=q.histo.end(); ++it)
		query.modify(it->first, it->second);
}

visual_database::visual_database(id_cluster_collection::query_flags flags) : id_cluster_collection(flags), db(0)
{
}

visual_database::~visual_database()
{
	for (stmt_cache_map::iterator it(stmt_cache.begin()); it!=stmt_cache.end(); ++it)
		sqlite3_finalize(it->second);

	for (image_cache_map::iterator it(image_cache.begin()); it!=image_cache.end(); ++it)
		cvReleaseImage(&it->second);

	if (db) sqlite3_close(db);
}

bool visual_database::open(const char *fn)
{
	sqlite3 *sql3;
	int rc = sqlite3_open(fn, &sql3);
	if (rc) {
		cerr << "Can't open database: " <<  sqlite3_errmsg(sql3) << endl;
		sqlite3_close(sql3);
		return false;
	}
	return connect_to_db(sql3);
}

bool visual_database::connect_to_db(sqlite3 *sql3db)
{
	if (!sql3db) return false;

	db = sql3db;

	// create tables
	char *errmsg;
	int rc = sqlite3_exec(db, 
		"create table if not exists images ("
			"img_id integer primary key autoincrement,"
			"width integer,"
			"height integer,"
			"step integer,"
			"channels integer,"
			"data blob"
		");\n"
		"create table if not exists Objects ("
			"obj_id integer primary key autoincrement,"
			"timestamp char(19) default CURRENT_TIMESTAMP,"
			"comment text,"
			"flags integer"
		");\n"
		"create table if not exists Keypoints ( "
			"kpt_id integer primary key autoincrement,"
			"obj_id integer,"
			"cid integer,"
			"img_id integer,"
			"u real,"
			"v real,"
			"scale real,"
			"orient real,"
			"patch blob"
		");\n"
		"CREATE INDEX IF NOT EXISTS keypoints_obj_idx on Keypoints (obj_id);\n"
		"PRAGMA synchronous=OFF;\n", 
		0, 0, &errmsg);
	if (rc != SQLITE_OK) {
		cerr << "Error while creating tables: " << errmsg << endl;
		sqlite3_free(errmsg);
		sqlite3_close(db);
		db=0;
		return false;
	}

	// Load the content of the database
	
	const char *query="select obj_id,comment,flags from Objects";
	sqlite3_stmt *stmt=0;
	const char *tail=0;
	rc = sqlite3_prepare_v2(db, query, -1, &stmt, &tail);

	if (rc != SQLITE_OK) {
		cerr << "Error: " << sqlite3_errmsg(db) << endl;
		cerr << "While compiling: " << query << endl;
		return false;
	}

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		sqlite3_int64 oid = sqlite3_column_int64(stmt, 0);
		visual_object *vo = new visual_object(this, oid, (const char *)sqlite3_column_text(stmt,1), sqlite3_column_int(stmt,2));

		sqlite3_stmt *kpt_stmt = get_cached_stmt("select cid, img_id, u, v, scale, orient, patch from Keypoints where obj_id=?");
		assert(kpt_stmt!=0);
		sqlite3_bind_int64(kpt_stmt, 1, oid);

		while (sqlite3_step(kpt_stmt) == SQLITE_ROW) {
			db_keypoint kpt( sqlite3_column_int(kpt_stmt, 0), // cid
					sqlite3_column_int64(kpt_stmt, 1)); // img_id
			kpt.u = float(sqlite3_column_double(kpt_stmt, 2));
			kpt.v = float(sqlite3_column_double(kpt_stmt, 3));
			kpt.scale = int(sqlite3_column_double(kpt_stmt, 4));
			kpt.descriptor.orientation = float(sqlite3_column_double(kpt_stmt, 5));

			const unsigned length = sizeof(kpt.descriptor._rotated);
			assert((unsigned)sqlite3_column_bytes(kpt_stmt, 6) == length);
			memcpy(kpt.descriptor._rotated, sqlite3_column_blob(kpt_stmt, 6), length);
			vo->representative_image = kpt.image;
			vo->add(kpt);
		}
		vo->prepare();
		add_to_index(vo);

		kpt_stmt = get_cached_stmt("select rowid, x, y, type, descr from annotations where obj=?", false);
		if (kpt_stmt!=0) {
			sqlite3_bind_int64(kpt_stmt, 1, oid);
			while (sqlite3_step(kpt_stmt) == SQLITE_ROW) {
				vo->annotations.push_back(visual_object::annotation(
							sqlite3_column_int64(kpt_stmt, 0), // id
							float(sqlite3_column_double(kpt_stmt, 1)), // x
							float(sqlite3_column_double(kpt_stmt, 2)), // y
							(const char *)sqlite3_column_text(kpt_stmt, 4), // descr
							 sqlite3_column_int(kpt_stmt, 3))); // type
			}
		}
	}

	sqlite3_finalize(stmt);

	version++;
	return true;
}

img_id visual_database::add_image(IplImage *_im)
{
	assert(_im!=0);
	IplImage *im = cvCloneImage(_im);

	const char *query = "insert into images (width, height, step, channels, data) values (?,?,?,?,?)";
	sqlite3_stmt *stmt=get_cached_stmt(query);
	assert(stmt!=0);

	int rc = sqlite3_bind_int(stmt, 1, im->width);
	assert(rc == SQLITE_OK);
	rc = sqlite3_bind_int(stmt, 2, im->height);
	assert(rc == SQLITE_OK);
	rc = sqlite3_bind_int(stmt, 3, im->widthStep);
	assert(rc == SQLITE_OK);
	rc = sqlite3_bind_int(stmt, 4, im->nChannels);
	assert(rc == SQLITE_OK);
	rc = sqlite3_bind_blob(stmt, 5, im->imageData, im->height*im->widthStep, SQLITE_STATIC);
	assert(rc == SQLITE_OK);

	if(sqlite3_step(stmt)!=SQLITE_DONE) {
		printf("Error message: %s\n", sqlite3_errmsg(db));
	}
	img_id id = sqlite3_last_insert_rowid(db);
	image_cache[id] = im;
	return id;
}

IplImage *visual_database::get_image(img_id img)
{
	image_cache_map::iterator it = image_cache.find(img);
	if (it != image_cache.end())
		return it->second;

	const char *query = "select width,height,step,channels,data from images where img_id = ?";
	sqlite3_stmt *stmt= get_cached_stmt(query);
	assert(stmt!=0);

	IplImage *result=0;

	int rc = sqlite3_bind_int64(stmt, 1, img);
	assert(rc == SQLITE_OK);

	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
		CvMat m;
		int width = sqlite3_column_int(stmt, 0);
		int height = sqlite3_column_int(stmt, 1);
		int step = sqlite3_column_int(stmt, 2);
		int channels = sqlite3_column_int(stmt, 3);
		void *data = const_cast<void *>(sqlite3_column_blob(stmt, 4));

		assert(sqlite3_column_bytes(stmt, 4) == step*height);

		cvInitMatHeader(&m, height, width, 
				CV_MAKETYPE(CV_8U, channels), 
				data, step);

		IplImage header;
		result = cvCloneImage(cvGetImage(&m,&header));
		image_cache[img] = result;
	} else if (rc == SQLITE_DONE) {
		// not found.. return 0.
		cerr << "Image " << img << " not found !\n";
	} else {
		cerr << "Error while searching for image " << img << ": " << sqlite3_errmsg(db) << endl;
	}
	return result;
}

visual_object *visual_database::create_object(const char *comment, int flags)
{
	const char *query = "insert into Objects (comment, flags) values (?,?)";
	sqlite3_stmt *stmt= get_cached_stmt(query);
	assert(stmt != 0);

	sqlite3_bind_text(stmt, 1, (comment? comment:""), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 2, flags);
	if (sqlite3_step(stmt) != SQLITE_DONE) {
		printf("Error message: %s\n", sqlite3_errmsg(db));
		return 0;
	}
	visual_object *obj = new visual_object(this, sqlite3_last_insert_rowid(db), comment, flags);
	version++;
	return obj;
}

bool visual_database::remove_object(visual_object *obj)
{
	// destroy data on disk first

	exec_sql("begin");

	sqlite3_stmt *stmt= get_cached_stmt("delete from objects where obj_id=?");
	assert(stmt != 0);
	sqlite3_bind_int64(stmt, 1, obj->obj_id);

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		printf("Error message: %s\n", sqlite3_errmsg(db));
		return false;
	}

	stmt= get_cached_stmt("delete from keypoints where obj_id=?");
	assert(stmt != 0);
	sqlite3_bind_int64(stmt, 1, obj->obj_id);

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		printf("Error message: %s\n", sqlite3_errmsg(db));
		return false;
	}

	stmt= get_cached_stmt("delete from images where img_id=?");
	assert(stmt != 0);
	sqlite3_bind_int64(stmt, 1, obj->representative_image);

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		printf("Error message: %s\n", sqlite3_errmsg(db));
		return false;
	}

	exec_sql("commit");

	// now update the RAM index
	remove_cluster(obj);

	// cleaning
	obj->obj_id = -1;
	obj->representative_image=-1;
	obj->clear();

	return true;
}


sqlite3_stmt *visual_database::get_cached_stmt(const char *query, bool verbose)
{
	assert(db!=0);
	stmt_cache_map::iterator it(stmt_cache.find(query));
	if (it != stmt_cache.end()) {
		sqlite3_reset(it->second);
		sqlite3_clear_bindings(it->second);
		return it->second;
	}

	sqlite3_stmt *stmt=0;
	const char *tail;

	int rc = sqlite3_prepare_v2(db, query, -1, &stmt, &tail);

	if (rc != SQLITE_OK) {
		if (verbose) {
			cerr << "Error: " << sqlite3_errmsg(db) << endl;
			cerr << "While compiling: " << query << endl;
		}
		return 0;
	}
	stmt_cache[query] = stmt;

	return stmt;
}

bool visual_database::exec_sql(const char *query)
{
	sqlite3_stmt *stmt = get_cached_stmt(query);
	if (stmt==0) return false;

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		printf("Error message: %s\n", sqlite3_errmsg(db));
		return false;
	}
	return true;
}

db_keypoint *visual_object::add_keypoint(pyr_keypoint *p, img_id img)
{
	if (!p) return 0;
	assert(this!=0);

	db_keypoint db_p(*p, img);

	if (p->frame && ((pyr_frame*)p->frame)->tracker->id_clusters == 0)
		db_p.cid = p->id;

	if (db_p.cid ==0) return 0;

	// add the cid to the visual object cid histogram.
	vdb->update_cluster(this, db_p.cid, 1); 
	db_keypoint *ret = const_cast<db_keypoint *>(&(*points.insert(db_p)));

	sqlite3_stmt *stmt= vdb->get_cached_stmt(
			"insert into Keypoints (obj_id, cid, img_id, u, v, scale, orient, patch) values (?,?,?,?,?,?,?,?)");
	assert(stmt != 0);
	int rc = sqlite3_bind_int64(stmt, 1, obj_id);
	assert(rc==SQLITE_OK);
	rc = sqlite3_bind_int(stmt, 2, db_p.cid);
	assert(rc==SQLITE_OK);
	rc = sqlite3_bind_int64(stmt, 3, img);
	assert(rc==SQLITE_OK);
	rc = sqlite3_bind_double(stmt, 4, db_p.u);
	assert(rc==SQLITE_OK);
	rc = sqlite3_bind_double(stmt, 5, db_p.v);
	assert(rc==SQLITE_OK);
	rc = sqlite3_bind_double(stmt, 6, db_p.scale);
	assert(rc==SQLITE_OK);
	rc = sqlite3_bind_double(stmt, 7, db_p.descriptor.orientation);
	assert(rc==SQLITE_OK);
	sqlite3_bind_blob(stmt, 8, p->descriptor._rotated, sizeof(p->descriptor._rotated), SQLITE_TRANSIENT);

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		printf("Error message: %s\n", sqlite3_errmsg(vdb->db));
	}
	return ret;
}

unsigned visual_object::add_frame(pyr_frame *frame)
{
	unsigned r=0;
	vdb->exec_sql("begin");
	img_id img = vdb->add_image(frame->pyr->images[0]);
	representative_image = img;

	for (tracks::keypoint_frame_iterator it=frame->points.begin(); !it.end(); ++it) 
	{
		pyr_keypoint * p = (pyr_keypoint *)  it.elem();
		add_keypoint(p,img);
	}
	vdb->exec_sql("commit");
	vdb->version++;
	return r;
}

float visual_object::get_correspondences(pyr_frame *frame, correspondence_vector &corresp)
{
	float r = get_correspondences_std(frame, corresp);
	if (flags & (VERIFY_HOMOGRAPHY | VERIFY_FMAT))
		r = verify(frame, corresp);
	return r;
}

float visual_object::get_correspondences_std(pyr_frame *frame, correspondence_vector &corresp)
{
	corresp.clear();
	corresp.reserve(frame->points.size()*4);

	double score=0;
	double max_score=0;

	int npts=0;

	//vdb->idf_normalize();
	//max_score = weighted_sum;

	for (uumap::iterator i(histo.begin()); i!=histo.end(); ++i) {
		max_score += /* i->second */ vdb->idf(i->first);
	}
	//cout << "max_score: " << max_score << ", weighted_sum: " << weighted_sum << endl;
	std::set<unsigned> matched_cids;

	for (tracks::keypoint_frame_iterator it(frame->points.begin()); !it.end(); ++it) {
		pyr_keypoint *k = (pyr_keypoint *) it.elem();

		if (((pyr_frame *)k->frame)->tracker->id_clusters==0) {

			db_keypoint_vector::iterator begin, end;
			find(k->id,begin,end);
			for (db_keypoint_vector::iterator i(begin); i!=end; i++) {
				corresp.push_back(correspondence(const_cast<db_keypoint *>(&(*i)), k, 1));
				if (matched_cids.insert(k->id).second) {
					id_cluster_collection::id2cluster_map::iterator id_it = vdb->id2cluster.find(k->id);
					if (id_it != vdb->id2cluster.end())
						score += vdb->idf(id_it); 
				}
			}
		} else {


		pyr_track *track = (pyr_track *) k->track;
		if (track ==0 || k->cid==0) continue;

		npts++;

		bool added=false;
		track->id_histo.sort_results_min_ratio(.7);
		for(incremental_query::iterator it(track->id_histo.begin()); it!=track->id_histo.end(); ++it)
		{
			int cid = it->c->id;
			db_keypoint_vector::iterator begin, end;
			find(cid,begin,end);

			if (begin == end) continue;

			float idf=1;
			id_cluster_collection::id2cluster_map::iterator id_it = vdb->id2cluster.find(cid);
			if (id_it != vdb->id2cluster.end())
				idf = vdb->idf(id_it);

			for (db_keypoint_vector::iterator i(begin); i!=end; i++) 
			{
				corresp.push_back(correspondence(const_cast<db_keypoint *>(&(*i)), k, 1));
				if (!added) {
					added=true;
					if (matched_cids.insert(cid).second) 
						score += idf;
				}
			}
		}
		}
	}
	//return 2.0f*score/(float)(min((int)points.size(),npts));
	//return matched_cids.size()/(float)ncids;
	return float (score/max_score);
}

float visual_object::verify(pyr_frame *, correspondence_vector &corresp)
{

	if ((flags & (VERIFY_HOMOGRAPHY | VERIFY_FMAT)) == 0) return 0;
	if (corresp.size() < 16) return 0;

	CvMat *frame_pts = cvCreateMat(corresp.size(), 3, CV_32FC1);
	CvMat *obj_pts = cvCreateMat(corresp.size(), 3, CV_32FC1);
	CvMat *mask = cvCreateMat(1, corresp.size(), CV_8UC1);


	float *f = frame_pts->data.fl;
	float *o = obj_pts->data.fl;
	for (correspondence_vector::iterator it(corresp.begin()); it!=corresp.end(); ++it) {
		*f++ = it->frame_kpt->u;
		*f++ = it->frame_kpt->v;
		*f++ = 1;
		*o++ = it->obj_kpt->u;
		*o++ = it->obj_kpt->v;
		*o++ = 1;
	}

	if (M==0) M = cvCreateMat(3,3,CV_64FC1);

	int r = 0;
	if (flags & VERIFY_HOMOGRAPHY) {
		r = cvFindHomography(obj_pts, frame_pts, M, CV_RANSAC, 5, mask);
	} else {
		r = cvFindFundamentalMat(obj_pts, frame_pts, M, CV_FM_RANSAC, 5, .99, mask);
	}
	if (r!=1) {
		cvReleaseMat(&mask);
		cvReleaseMat(&frame_pts);
		cvReleaseMat(&obj_pts);
		corresp.clear();
		return 0;
	}

	unsigned char *c = mask->data.ptr;
	int cnt=0;
	for (unsigned i=0; i<corresp.size(); i++)
		if (c[i]) cnt++;
	correspondence_vector filtered;
	filtered.reserve(cnt);
	correspondence_vector::iterator it = corresp.begin();

	for (unsigned i=0; i<corresp.size(); ++i, ++it)
		if (c[i]) filtered.push_back(*it);

	cout << "Filtered: " << filtered.size() << ", initial: " << corresp.size() << endl;
	filtered.swap(corresp);

	cvReleaseMat(&mask);
	cvReleaseMat(&frame_pts);
	cvReleaseMat(&obj_pts);
	return cnt / (float)filtered.size();
}

// annotation stuff
void visual_object::add_annotation(float x, float y, int type, const std::string &descr)
{
	sqlite3_stmt *stmt=vdb->get_cached_stmt("create table if not exists annotations (obj integer, x real, y real, type integer, descr text)");
	if(sqlite3_step(stmt)!=SQLITE_DONE) {
		printf("Error message: %s\n", sqlite3_errmsg(vdb->db));
		return;
	}

	assert(stmt!=0);
	const char *query = "insert into annotations (obj, x, y, type, descr) values (?,?,?,?,?)";
	stmt=vdb->get_cached_stmt(query);
	assert(stmt!=0);

	sqlite3_bind_int64(stmt, 1, obj_id);
	sqlite3_bind_double(stmt, 2, x);
	sqlite3_bind_double(stmt, 3, y);
	sqlite3_bind_int(stmt, 4, type);
	sqlite3_bind_text(stmt, 5, descr.c_str(), -1, SQLITE_TRANSIENT);

	if(sqlite3_step(stmt)!=SQLITE_DONE) {
		printf("Error message: %s\n", sqlite3_errmsg(vdb->db));
	}

	annotations.push_back(annotation(sqlite3_last_insert_rowid(vdb->db), x, y, descr, type));
}

