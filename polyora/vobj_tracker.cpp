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
#include <iostream>
#include <algorithm>
#include "vobj_tracker.h"
#include "homography4.h"
#include "timer.h"

#ifdef min
#undef min
#endif

static vobj_keypoint::vobj_keypoint_factory_t default_vobj_keypoint_factory;
static vobj_frame::vobj_frame_factory_t default_vobj_frame_factory;

namespace {
bool homography_is_plausible(float H[3][3]) {
    float trace = H[0][0] + H[1][1];
    float half_trace = trace * 0.5f;
    float det = H[0][0] * H[1][1] - H[0][1] * H[1][0];
    float delta = sqrtf(trace * trace * .25f - det);
    float l1 = half_trace + delta;
    float l2 = half_trace - delta;
    float ratio = l1 / l2;
    return  (l1 < 3*l2 && l2 < 3*l1);
}
}

vobj_tracker::vobj_tracker(int width, int height, int levels, int max_motion,
		visual_database *vdb, bool glContext,
		pyr_keypoint::pyr_keypoint_factory_t *kf,
		vobj_frame::vobj_frame_factory_t *ff,
		pyr_track::pyr_track_factory_t *tf )
	: kpt_tracker(width, height, levels, max_motion, glContext,
			(kf?kf:&default_vobj_keypoint_factory),
			(ff?ff:&default_vobj_frame_factory),
			tf),
			//(tf?ff:&default_vobj_track_factory)),
	vdb(vdb)
{
	score_threshold=.00;
	homography_corresp_threshold = 10;
	fmat_corresp_threshold = 20;
	max_results=3;
	use_incremental_learning=true;
}

pyr_frame *vobj_tracker::process_frame(IplImage *im)
{
	vobj_frame *frame = static_cast<vobj_frame *>(kpt_tracker::process_frame(im));
	vobj_frame *last_frame = static_cast<vobj_frame *>(get_nth_frame(1));
	track_objects(frame, last_frame);
	if (use_incremental_learning)
		incremental_learning(frame, 5, 30, 3000);
	return frame;
}

pyr_frame *vobj_tracker::process_frame_pipeline(IplImage *im)
{
	pyr_frame *new_f=0;
	if (im) new_f = create_frame(im);
#pragma omp parallel 
	{
#pragma omp master
	{
	if (im) {
		//TaskTimer::pushTask("pyramid");
		new_f->pyr->build();
		//TaskTimer::popTask();
		//TaskTimer::pushTask("Feature detection");
		detect_keypoints(new_f);
		//TaskTimer::popTask();
	}
	}
#pragma omp single
	{
	if (pipeline_stage1) {
		traverse_tree(pipeline_stage1);
		pipeline_stage1->append_to(*this);
		vobj_frame *last_frame = static_cast<vobj_frame *>(get_nth_frame(1));
		track_ncclk(pipeline_stage1, last_frame);
		track_objects(static_cast<vobj_frame *>(pipeline_stage1), last_frame);
		if (use_incremental_learning)
			incremental_learning(static_cast<vobj_frame *>(pipeline_stage1), 5, 30, 3000);
	}
	}
	}
	pyr_frame *out = pipeline_stage1;
	pipeline_stage1 = new_f;
	return out;
}



/* algorithm:
   - seek for visual object candidates: prev frame objects, and db query.
   - for each candidate, in order:
   - build an ordered corresp list with unattributed features
   - PROSAC
 */
int vobj_tracker::track_objects(vobj_frame *frame, vobj_frame *last_frame)
{

	TaskTimer::pushTask("track_objects");

	TaskTimer::pushTask("find_candidates");
	std::set<visual_object *> candidates;
	find_candidates(frame, candidates, last_frame);
	TaskTimer::popTask();

	TaskTimer::pushTask("verify");
	for (std::set<visual_object *>::iterator it(candidates.begin()); it!= candidates.end(); ++it)
	{
		vobj_instance instance;
		if (verify(frame, *it, &instance)) {
			// found object!
			frame->visible_objects.push_back(instance);
		}
	}
	TaskTimer::popTask();

	TaskTimer::popTask();
	return frame->visible_objects.size();
}
	   
void vobj_tracker::find_candidates(vobj_frame *frame, std::set<visual_object *> &candidates, vobj_frame *last_frame)
{
	candidates.clear();
	if (frame==0) return;

	if (last_frame)
	for (vobj_instance_vector::const_iterator it(last_frame->visible_objects.begin()); it!=last_frame->visible_objects.end(); ++it)
	{
		assert(it->object != 0);
		candidates.insert(it->object);
	}

	// query the database
	if (query.get() == 0) {
		query.reset(vdb->create_incremental_query());
		init_query_with_frame(*query, frame);
	} else {
		//init_query_with_frame(*query, frame);
		update_query_with_frame(*query, this);
	}

	for (incremental_query::iterator it(query->sort_results(max_results+candidates.size())); it != query->end() && it->score > score_threshold; ++it)
	{
		candidates.insert(static_cast<visual_object *>(it->c));
	}

	//std::cout << "Found " << candidates.size() << " candidates.\n";
}

int get_correspondences(vobj_frame *frame, visual_object *obj, visual_object::correspondence_vector &corresp)
{
	corresp.clear();
	corresp.reserve(frame->points.size()*4);

	double score=0;
	double max_score=0;

	int npts=0;

	int nb_tracked=0;

	//vdb->idf_normalize();
	//max_score = weighted_sum;

	for (id_cluster::uumap::iterator i(obj->histo.begin()); i!=obj->histo.end(); ++i) {
		max_score += /* i->second */ obj->vdb->idf(i->first);
	}
	//cout << "max_score: " << max_score << ", weighted_sum: " << weighted_sum << endl;
	std::set<unsigned> matched_cids;

	for (tracks::keypoint_frame_iterator it(frame->points.begin()); !it.end(); ++it) {
		vobj_keypoint *k = (vobj_keypoint *) it.elem();

		vobj_keypoint *prev = k->prev_match_vobj();
		if (prev && prev->vobj && prev->obj_kpt)
		{
			if (prev->vobj == obj) {
				// the point was matched on previous frame
				corresp.push_back(visual_object::correspondence(prev->obj_kpt, k, 100));
				nb_tracked++;
			} // else: the point was tracked and belongs to another object.
			continue;
		} 
		// the point is already matched with another object
		if (k->vobj) continue;

		// the track is too short
		if (!k->track_is_longer(2)) continue;

		visual_database *vdb = obj->vdb;

		if (((pyr_frame *)k->frame)->tracker->id_clusters==0) {

			visual_object::db_keypoint_vector::iterator begin, end;
			obj->find(k->id,begin,end);
			for (visual_object::db_keypoint_vector::iterator i(begin); i!=end; i++) {
                                corresp.push_back(visual_object::correspondence(const_cast<db_keypoint*>(&(*i)), k, .1));
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
			visual_object::db_keypoint_vector::iterator begin, end;
			obj->find(cid,begin,end);

			if (begin == end) continue;

			float idf=1;
			id_cluster_collection::id2cluster_map::iterator id_it = vdb->id2cluster.find(cid);
			if (id_it != vdb->id2cluster.end())
				idf = vdb->idf(id_it);

			float s = it->score * idf;
			for (visual_object::db_keypoint_vector::iterator i(begin); i!=end; i++) 
			{
				corresp.push_back(visual_object::correspondence(const_cast<db_keypoint *>(&(*i)), k, s));
				if (!added) {
					added=true;
					if (matched_cids.insert(cid).second) 
						score += idf;
				}
			}
		}
		}
	}
	return nb_tracked;
}

static point2d transform(CvMat *H, point2d &a)
{
	float *h = H->data.fl;
	float m[3];
	for (int i=0; i<3; i++)
		m[i] = h[i*3]*a.u + h[i*3+1]*a.v + h[i*3+2];
	return point2d(m[0]/m[2], m[1]/m[2]);
}

static bool check_epipolar_constraint(const point2d &a, const CvMat *M, const point2d &b, float threshold=3.0f)
{
	float *h = M->data.fl;
	float m[3];
	for (int i=0; i<3; i++)
		m[i] = h[i*3]*b.u + h[i*3+1]*b.v + h[i*3+2];

	float d = m[0]*a.u + m[1]*a.v + m[2];

	return fabsf(d) < threshold;
}

bool vobj_tracker::verify(vobj_frame *frame, visual_object *obj, vobj_instance *instance)
{

	instance->object=0;
	if ((obj->get_flags() & (visual_object::VERIFY_HOMOGRAPHY | visual_object::VERIFY_FMAT)) == 0) return false;

	TaskTimer::pushTask("get_correspondences");
	visual_object::correspondence_vector corresp;
	int nb_tracked=get_correspondences(frame, obj, corresp);
	TaskTimer::popTask();

	int n_corresp = corresp.size();

	if (nb_tracked >= 10) n_corresp = std::min((int)(1.2*nb_tracked), n_corresp);

	if (n_corresp < 10) return 0;

	CvMat *frame_pts = cvCreateMat(corresp.size(), 2, CV_32FC1);
	CvMat *obj_pts = cvCreateMat(corresp.size(), 2, CV_32FC1);
	CvMat *mask = cvCreateMat(1, corresp.size(), CV_8UC1);

	float *f = frame_pts->data.fl + nb_tracked*2;
	float *o = obj_pts->data.fl + nb_tracked*2;
	float *f_t = frame_pts->data.fl;
	float *o_t = obj_pts->data.fl;
	
	for (visual_object::correspondence_vector::iterator it(corresp.begin()); it!=corresp.end(); ++it) {
		if (it->correl >=100 ) {
			// point was tracked
			*f_t++ = it->frame_kpt->u;
			*f_t++ = it->frame_kpt->v;
			//*f_t++ = 1;
			*o_t++ = it->obj_kpt->u;
			*o_t++ = it->obj_kpt->v;
			//*o_t++ = 1;
		} else {
			// point was matched
			*f++ = it->frame_kpt->u;
			*f++ = it->frame_kpt->v;
			//*f++ = 1;
			*o++ = it->obj_kpt->u;
			*o++ = it->obj_kpt->v;
			//*o++ = 1;
		}
	}
	obj_pts->rows = frame_pts->rows = mask->cols = n_corresp;

	//std::cout << "  " << nb_tracked << " tracked features, out of " << corresp.size() << " matches. Using only " << n_corresp << " matches.\n";

	CvMat tmat = instance->get_transform();
	CvMat *M = &tmat;

	int r = 0;
	int support = -1;
	TaskTimer::pushTask("FindHomography");
	if (nb_tracked>=10) TaskTimer::pushTask("Track");
	else TaskTimer::pushTask("Detect");
	if (obj->get_flags() & visual_object::VERIFY_HOMOGRAPHY) {
		if (nb_tracked <10) {
			if (0)
				r = cvFindHomography(obj_pts, frame_pts, M, CV_RANSAC, 6, mask);
			else {
				support = ransac_h4(
						obj_pts->data.fl, obj_pts->step, 
						frame_pts->data.fl, frame_pts->step, 
						obj_pts->rows,
						1000, // max iter, actually 4 times more
						6, // distance threshold
						50, // stop if we find 50 matches
						instance->transform,
						0, // inliers mask
						obj_pts->data.fl,frame_pts->data.fl);
				//std::cout << "Support: " << r << std::endl;
				if (support >= homography_corresp_threshold) {
					// refine with LMEDS
					obj_pts->rows = support;
					frame_pts->rows = support;
					if (homography_is_plausible(instance->transform)) {
					    r = cvFindHomography(obj_pts, frame_pts, M, CV_LMEDS);
					}
				} else {
					r=0;
				}
			}
		} else {
			if (0) {
				r = cvFindHomography(obj_pts, frame_pts, M, CV_LMEDS, 6, mask);
			} else {
				support = ransac_h4(
						obj_pts->data.fl, obj_pts->step, 
						frame_pts->data.fl, frame_pts->step, 
						obj_pts->rows,
						100, // max iter
						3.5, // distance threshold
						100, // stop if we find enough matches
						instance->transform,
						0, // inliers mask
						obj_pts->data.fl,frame_pts->data.fl);
				// refine result
				obj_pts->rows = support;
				frame_pts->rows = support;
				//r = (support > homography_corresp_threshold ? 1:0);
				r = cvFindHomography(obj_pts, frame_pts, M);
			}
		}
	} else {
		r = cvFindFundamentalMat(obj_pts, frame_pts, M, (nb_tracked>=16 ? CV_FM_LMEDS : CV_FM_RANSAC), 4, .99, mask);
	}
	TaskTimer::popTask();
	TaskTimer::popTask();
	
	if (r!=1) {
		cvReleaseMat(&mask);
		cvReleaseMat(&frame_pts);
		cvReleaseMat(&obj_pts);
		corresp.clear();
		return false;
	}

	int inliers=0;
	int inlier_threshold;
	if (obj->get_flags() & visual_object::VERIFY_HOMOGRAPHY) {
		// verify homography for all points
		inlier_threshold = homography_corresp_threshold;
		//std::cout << "Homography checking.\n";
		for (unsigned i=0; i<corresp.size(); ++i) {
			vobj_keypoint *k = static_cast<vobj_keypoint *>(corresp[i].frame_kpt);
			point2d p = transform(M, *corresp[i].obj_kpt);
			float dist = p.dist(*k);
			if (dist<6) {
				inliers++;
			} else {
				corresp[i].frame_kpt = 0;
				vobj_keypoint *prev = k->prev_match_vobj();
				if (prev && prev->vobj==obj) {
					if (0)
					std::cout << "  Point at " << k->u << "," << k->v 
						<< " was lost. Reproj error: " << dist << std::endl;
				}
			}
		}
	} else if (obj->get_flags() & visual_object::VERIFY_FMAT) {
		// fundamental matrix
		inlier_threshold = fmat_corresp_threshold;
		std::cout << "F-Mat checking.\n";
		for (unsigned i=0; i<corresp.size(); ++i) {
			vobj_keypoint *k = static_cast<vobj_keypoint *>(corresp[i].frame_kpt);
			if (check_epipolar_constraint(*corresp[i].obj_kpt, M, *k)) {
				inliers++;
			} else {
				corresp[i].frame_kpt=0;
			}
		}
	}



	if (0)
	std::cout << "  Checking object, " << inliers << " correct matches out of " 
		<< corresp.size() << "( used: " << n_corresp << " tracked: " << nb_tracked << ")\n";
	//	<<  " cvFindHomography returned: " << r << std::endl;
	bool success = (inliers>=inlier_threshold);
	if (success) instance->object = obj;
	instance->support = inliers;
	//std::cout << " success: " << success << std::endl;

	if (success) 
	for (unsigned i=0; i<corresp.size(); ++i) {
		if (corresp[i].frame_kpt) {
			vobj_keypoint *k = static_cast<vobj_keypoint *>(corresp[i].frame_kpt);
			k->vobj = obj;
			k->obj_kpt = corresp[i].obj_kpt;
		}
	}

	/*
	if (success)
		std::cout << "Matched " << inliers << " features, out of " << obj->nb_points() << " (" << 100.0f*inliers/obj->nb_points() 
			<< "%)\n";
	*/

	cvReleaseMat(&mask);
	cvReleaseMat(&frame_pts);
	cvReleaseMat(&obj_pts);
	return success;
}


void vobj_tracker::remove_visible_objects_from_db(vobj_frame *frame)
{
	if (frame->visible_objects.size()==0) return;

	for (vobj_instance_vector::iterator it(frame->visible_objects.begin());
			it != frame->visible_objects.end(); ++it)
	{
		vdb->remove_object(it->object);

		// TODO: implement garbage collection to avoid that previous
		// frames point at invalid objects.
		delete it->object;
	}

	for (tracks::keypoint_frame_iterator it(frame->points.begin()); !it.end(); ++it) {
		vobj_keypoint *k = (vobj_keypoint *) it.elem();
		k->vobj=0;
		k->obj_kpt=0;
	}
	frame->visible_objects.clear();
}

vobj_keypoint *vobj_frame::find_closest_match(float u, float v, float radius)
{
	float best_dist = radius*radius;
	vobj_keypoint *best_match=0;

	for (tracks::keypoint_frame_iterator it=points.search(u, v, radius); !it.end(); ++it) {
		vobj_keypoint *p = (vobj_keypoint *) it.elem();
		if (p->vobj) {
			float du = u-p->u;
			float dv = v-p->v;
			float d2 = du*du+dv*dv;

			if (d2<best_dist) {
				best_dist = d2;
				best_match = p;
			}
		}
	}
	return best_match;
}

vobj_instance *vobj_frame::find_instance(const visual_object *obj)
{
	for (vobj_instance_vector::iterator it=visible_objects.begin(); it!=visible_objects.end(); ++it)
		if (it->object == obj) return &(*it);
	return 0;
}

const vobj_instance *vobj_frame::find_instance(const visual_object *obj) const
{
	for (vobj_instance_vector::const_iterator it=visible_objects.begin(); it!=visible_objects.end(); ++it)
		if (it->object == obj) return &(*it);
	return 0;
}

static point2d back_project(const point2d &p, const float H[3][3])
{
	float su = p.u;
	float sv = p.v;

	float t4 = H[0][0]*H[1][1];
	float t6 = H[0][0]*H[1][2];
	float t8 = H[0][1]*H[1][0];
	float t10 = H[0][2]*H[1][0];
	float t12 = H[0][1]*H[2][0];
	float t14 = H[0][2]*H[2][0];
	float t17 = 1/(t4*H[2][2]-t6*H[2][1]-t8*H[2][2]+t10*H[2][1]+t12*H[1][2]-t14*H[1][1]);
	float r[3];
	r[0] = (H[1][1]*H[2][2]-H[1][2]*H[2][1])*t17*su-(H[0][1]*H[2][2]-H[0][2]*
			H[2][1])*t17*sv+(H[0][1]*H[1][2]-H[0][2]*H[1][1])*t17;
	r[1] = -(H[1][0]*H[2][2]-H[1][2]*H[2][0])*t17*su+(H[0][0]*H[2][2]-t14)*
		t17*sv-(t6-t10)*t17;
	r[2] = (H[1][0]*H[2][1]-H[1][1]*H[2][0])*t17*su-(H[0][0]*H[2][1]-t12)*t17
		*sv+(t4-t8)*t17;

	return point2d(r[0]/r[2], r[1]/r[2]);
}

static point2d homo_transform(const float h[3][3], const point2d &p) 
{
	float a = h[0][0]*p.u + h[0][1]*p.v + h[0][2];
	float b = h[1][0]*p.u + h[1][1]*p.v + h[1][2];
	float c = h[2][0]*p.u + h[2][1]*p.v + h[2][2];
	return point2d(a/c, b/c);
}

void vobj_tracker::incremental_learning(vobj_frame *frame, int track_length, float radius, int max_pts)
{
	if (frame->visible_objects.size()==0) 
		return; // nothing to update

	vdb->start_update();

	for (tracks::keypoint_frame_iterator it(frame->points.begin()); !it.end(); ++it) {
		vobj_keypoint *k = (vobj_keypoint *) it.elem();

		// skip uninteresting points
		if (k->vobj || k->obj_kpt || k->cid==0 || k->track_length() < track_length) {
			//std::cout << "skip 1: " << k->vobj << "," << k->obj_kpt << ", " << k->cid << ", track length:" << k->track_length() << "\n";
			continue; 
		}

		// search candidate object (from nearest validated match)
		vobj_keypoint *closest_match = frame->find_closest_match(k->u, k->v, radius);
		if (!closest_match) {
			//std::cout << "skip: can't find a close match.\n";
			continue;
		}
		visual_object *obj = closest_match->vobj;

		// incremental learning works only on planar objects
		if (!obj || (obj->get_flags() & visual_object::VERIFY_HOMOGRAPHY)==0) {
			//std::cout << "skip 2\n";
			continue;
		}

		// prevent the object from growing forever
		float filled_percent = obj->nb_points()/(float)max_pts;
		if ( filled_percent > ((float)rand()/(float)RAND_MAX)) continue;

		vobj_instance *base_instance = frame->find_instance(obj);

		point2d backproj = back_project(*k, base_instance->transform);

		bool ok=true;
		// iterate over the track, skipping the current frame
		tracks::keypoint_match_iterator t_it(k);
		for (--t_it; !t_it.end(); --t_it) {
			vobj_keypoint *t_k = (vobj_keypoint *) t_it.elem();
			vobj_frame *t_f = (vobj_frame *) t_k->frame;

			// compute reprojection error
			vobj_instance *inst = t_f->find_instance(obj);
			if (!inst || homo_transform(inst->transform, backproj).dist(*t_k) > 3) {
				ok = false;
				break;
			}
		}

		if (ok) {
			std::cout << "Adding keypoint at " << k->u <<"," << k->v << " backproj at " << backproj.u << ","<<backproj.v 
				<< ", nbkpt: " << obj->nb_points() << "/" << max_pts << std::endl;
			// add the point to the object!
			pyr_keypoint pk(*k);
			pk.u = backproj.u;
			pk.v = backproj.v;
			k->obj_kpt = obj->add_keypoint(&pk, obj->representative_image);
			if (k->obj_kpt)
				k->vobj = obj;
		}
	}

	vdb->finish_update();
}
