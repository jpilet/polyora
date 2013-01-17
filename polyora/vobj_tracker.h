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
#ifndef VOBJ_TRACKER
#define VOBJ_TRACKER

#include <memory>
#include <set>

#include "kpttracker.h"
#include "visual_database.h"

/*! \defgroup ObjectTrackingGroup Object level tracking
*/
/*@{*/

class vobj_instance
{
public:
	visual_object *object;
	int support;

	// Homography or F-mat
	float transform[3][3];
	CvMat get_transform() { CvMat m; cvInitMatHeader(&m, 3, 3, CV_32FC1, transform); return m; }
};
typedef std::vector<vobj_instance> vobj_instance_vector;

class vobj_keypoint : public pyr_keypoint 
{
public:
  // TODO(jpilet): replace visual_object with vobj_instance.
	visual_object *vobj;
	db_keypoint *obj_kpt;

	vobj_keypoint() : vobj(0), obj_kpt(0) {}

	vobj_keypoint *prev_match_vobj() { return static_cast<vobj_keypoint *>(matches.prev); }
	vobj_keypoint *next_match_vobj() { return static_cast<vobj_keypoint *>(matches.next); }

	virtual void dispose() { vobj=0; pyr_keypoint::dispose(); }

	struct vobj_keypoint_factory_t : pyr_keypoint_factory_t {
                virtual tkeypoint *create() { return new vobj_keypoint(); }
	};
};

/*
class vobj_track : pyr_track
{

	vobj_track(tracks *db) : pyr_track(db) {}

	struct vobj_track_factory_t : factory_t {
		virtual ttrack *create(tracks *db) { return new vobj_track(db); }
	};
};
*/

class vobj_frame : public pyr_frame
{
public:
	vobj_instance_vector visible_objects;

	vobj_frame(PyrImage *p, int bits=4) : pyr_frame(p, bits) { } 

	vobj_instance *find_instance(const visual_object *obj);
	const vobj_instance *find_instance(const visual_object *obj) const;
	vobj_keypoint *find_closest_match(float u, float v, float radius);

	struct vobj_frame_factory_t : pyr_frame_factory_t {
		virtual pyr_frame *create(PyrImage *p, int bits=4) { return new vobj_frame(p,bits); }
	};
};

class vobj_tracker : public kpt_tracker
{
public:

	visual_database *vdb;
	std::auto_ptr<incremental_query> query;

	float score_threshold;
	int max_results;

	//! Threshold to determine successful detection for homography. Default: 20
	int homography_corresp_threshold;

	//! Threshold to determine successful detection for fundamental matrix. Default: 20
	int fmat_corresp_threshold;

        vobj_tracker(int width, int height, int levels, int max_motion, visual_database *vdb=0, bool glContext=false,
                    pyr_keypoint::pyr_keypoint_factory_t *kf=0,
                    vobj_frame::vobj_frame_factory_t *ff=0,
                    pyr_track::pyr_track_factory_t *tf=0);
                    //vobj_track::vobj_track_factory_t *tf=0);

	virtual pyr_frame *process_frame(IplImage *im, long long timestamp);
	virtual pyr_frame *process_frame_pipeline(IplImage *im, long long timestamp);
	int track_objects(vobj_frame *frame, vobj_frame *last_frame);

	void remove_visible_objects_from_db(vobj_frame *frame);
	void incremental_learning(vobj_frame *frame, int track_length, float radius, int max_pts);

	bool use_incremental_learning;
protected:

	void find_candidates(vobj_frame *frame, std::set<visual_object *> &candidates, vobj_frame *last_frame);
	bool verify(vobj_frame *frame, visual_object *obj, vobj_instance *instance);
};

/*@}*/
#endif
