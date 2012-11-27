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
#ifndef KPT_TRACKER_H
#define KPT_TRACKER_H

#include <stack>

#include <opencv2/core/core.hpp>
#include <opencv2/core/core_c.h>

#include "yape.h"
#include "tracks.h"
#include "patchtagger.h"
#include "idcluster.h"
#include "sqlite3.h"

/*! \defgroup KptTrackingGroup Keypoint detection and tracking
*/
/*@{*/

// To activate SiftGPU, uncomment the following and undef WITH_YAPE
//#define WITH_SIFTGPU
#define WITH_YAPE

// These detectors are roughly implemented and do not work well.
//#define WITH_FAST
//#define WITH_GOOD_FEATURES_TO_TRACK
//#define WITH_ADAPT_THRESH
//#define WITH_SURF
//#define WITH_MSER

#if defined(WITH_YAPE) || defined(WITH_GOOD_FEATURES_TO_TRACK) \
	|| defined(WITH_FAST) || defined(WITH_ADAPT_THRESH) || defined(WITH_MSER)
#define WITH_PATCH_TAGGER_DESCRIPTOR
#define WITH_PATCH_AS_DESCRIPTOR
#endif


#include "kmeantree.h"

#ifdef WITH_SIFTGPU
#ifdef WIN32
#include <GL/glew.h>
#include <windows.h>
#endif
#include <SiftGPU.h>
#ifdef MACOS
#include <OpenGL/gl.h>
#else

#include <GL/gl.h>
#endif
#endif

#ifdef WITH_MSER
#include "mserdetector.h"
#endif

class kpt_tracker;

//! Stores a frame with its pyramid image
struct pyr_frame : tframe {
	PyrImage *pyr;
        std::vector<cv::Mat> cv_pyramid;
	kpt_tracker *tracker;

	pyr_frame(PyrImage *p, int bits=4);  
	virtual ~pyr_frame();
	virtual void append_to(tracks &t);

	struct pyr_frame_factory_t : factory_t {
		virtual pyr_frame *create(PyrImage *p, int bits=4) { return new pyr_frame(p,bits); }
	};
};

//! A keypoint detected in a pyramid, with orientation, scale and description.
struct pyr_keypoint : tkeypoint {
	int scale;
	point2d level;
	CvMat patch;

	//! feature strength (related to local contrast)
	float score;
	unsigned char *data;
	float mean, stdev;
	int id;
	unsigned cid;
	float cscore;

#ifdef WITH_SURF
	kmean_tree::descriptor_t surf_descriptor;
#endif
#ifdef WITH_SIFTGPU
	kmean_tree::descriptor_t sift_descriptor;
#endif
#ifdef WITH_MSER
	MSERegion mser;
	bool get_mser_patch(PyrImage &pyr, cv::Mat mat);
#endif

	patch_tagger::descriptor descriptor;
	kmean_tree::node_t *node;

	pyr_keypoint() { data=0; node=0; }
        pyr_keypoint(const pyr_keypoint &a);

	void set(tframe *f, keypoint &pt, int patch_size);
	void set(tframe *f, float u, float v, int scale, int patch_size);

	void prepare_patch(int size);
	virtual ~pyr_keypoint();

	virtual void dispose();

	const pyr_keypoint &operator = (const pyr_keypoint &a);

        struct pyr_keypoint_factory_t : factory_t {
                virtual tkeypoint *create() { return new pyr_keypoint(); }
        };
};

//! Collects track statistics for incremental cluster matching
struct pyr_track : ttrack {

	incremental_query id_histo;

        int nb_lk_tracked;

	//! called on point addition 
	virtual void point_added(tkeypoint *p);

	//! called on point removal
	virtual void point_removed(tkeypoint *p);

	pyr_track(tracks *db);

	struct pyr_track_factory_t : factory_t {
		virtual ttrack *create(tracks *db) { return new pyr_track(db); }
	};
};

/*! Scale-space feature tracker.
 */
class kpt_tracker : public tracks {
public:


        kpt_tracker(int width, int height, int levels, int max_motion , bool glContext=false,
                    pyr_keypoint::pyr_keypoint_factory_t *kf=0,
                    pyr_frame::pyr_frame_factory_t *ff=0,
                    pyr_track::pyr_track_factory_t *tf = 0);
	virtual ~kpt_tracker();

	bool load_from_db(sqlite3 *db);
	bool load_from_db(const char *dbfile);
	bool load_tree(sqlite3 *db);
	bool load_tree(const char *fn);
	bool load_clusters(sqlite3 *db);
	bool load_clusters(const char *fn);

	void set_size(int width, int height, int levels, int max_motion);

	/*! Store a new frame, detect features, and match them with previous frame.
	 *  This method takes care of releasing im.
	 */
	virtual pyr_frame *process_frame(IplImage *im);

	/*! Pipelined version of process_frame. 
	 *  Returns 0 on the first call.
	 *  This method takes care of releasing im.
	 */
	virtual pyr_frame *process_frame_pipeline(IplImage *im);

protected:
        pyr_frame *create_frame(IplImage *im);
        static void buildPyramid(pyr_frame *frame);
public:
	pyr_frame *add_frame(IplImage *im);
	void traverse_tree(pyr_frame *frame);
	void detect_keypoints(pyr_frame *f);
	void track_ncclk(pyr_frame *f, pyr_frame *lf);

	//! remove all frames, all keypoints, and all tracks.
	void clear();


#ifdef WITH_YAPE
	pyr_yape *detector;
	keypoint *points;
#endif

#ifdef WITH_SIFTGPU
	SiftGPU sift;
#endif

#ifdef WITH_MSER
	MSERDetector mser;
#endif

#ifdef WITH_ADAPT_THRESH
	CvMat *adapt_thresh_mat;
#endif

	kmean_tree::node_t *tree;
	id_cluster_collection *id_clusters;

	int nb_points;
	int max_motion;
	int nb_levels;
	int patch_size;
	float ncc_threshold, ncc_threshold_high;

	pyr_keypoint *best_match(pyr_keypoint *templ, tracks::keypoint_frame_iterator it);

private:

	class recycler_t {
	public:
		recycler_t(pyr_keypoint::pyr_keypoint_factory_t *f) : factory(f) {}

		pyr_keypoint *get_new() { 
//#ifdef _OPENMP
#if 1
			return (pyr_keypoint *)factory->create();
#else
			pyr_keypoint *r=0;
			if (!available.empty()) { pyr_keypoint *o = available.top(); available.pop(); r= o; }
			else r= (pyr_keypoint *)factory->create() ;
			return r;
#endif
		}
//#ifdef _OPENMP
#if 1
		void recycle(pyr_keypoint *obj) { delete (obj); }
#else
		void recycle(pyr_keypoint *obj) { available.push(obj); }
#endif
		void clear() {
			while (!available.empty()) { delete available.top(); available.pop(); }
		}
		~recycler_t() { clear(); }
	protected:
		std::stack<pyr_keypoint *> available;
		pyr_keypoint::pyr_keypoint_factory_t *factory;
	};

	recycler_t kpt_recycler;
	friend struct pyr_keypoint;

protected:
	pyr_frame *pipeline_stage1;
};

/*@}*/
#endif
