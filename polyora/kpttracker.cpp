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
using namespace std;
#include "kpttracker.h"
#ifdef WITH_FAST
#include "fast.h"
#endif
#include <stack>
#ifdef _OPENMP
#include <omp.h>
#endif
#include "timer.h"

#include <opencv2/video/tracking.hpp>

#ifdef WITH_ADAPT_THRESH
#include <adapt_thresh.h>
#endif

#include "pca_descriptor.h"

#ifdef WIN32
static inline double drand48() {
	return (double)rand()/(double)RAND_MAX;
}
// win32 compatibility
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#endif

int ncc_calls=0;

float cmp_ncc(pyr_keypoint *a, pyr_keypoint *b);

void pyr_keypoint::dispose() {
	pyr_frame *f = static_cast<pyr_frame *>(frame);
	unlink();
	if (f)
		f->tracker->kpt_recycler.recycle(this);
	else
		delete this;
}

kpt_tracker::~kpt_tracker() {
	clear();
	kpt_recycler.clear();
	if (pipeline_stage1) delete pipeline_stage1;

	if (tree) delete tree;
	if (id_clusters) delete id_clusters;
#ifdef WITH_YAPE
	if (detector) delete detector;
	if (points) delete[] points;
#endif
#ifdef WITH_ADAPT_THRESH
	cvReleaseMat(&adapt_thresh_mat);
#endif
}

void kpt_tracker::clear() {
	while (frames) remove_unmatched_tracks(frame_iterator(this));
}

static void license() {
	static bool printed=false;
	if (!printed) {
		printed=true;
		std::cerr << 
"  Polyora, Copyright (c) 2010 Julien Pilet\n"
"  Polyora comes with ABSOLUTELY NO WARRANTY; see LICENSE.txt.\n"
"  This is free software, and you are welcome to redistribute it under certain conditions; see LICENSE.txt.\n";
	}
}

pyr_keypoint::pyr_keypoint_factory_t default_pyr_keypoint_factory;
pyr_frame::pyr_frame_factory_t default_pyr_frame_factory;
pyr_track::pyr_track_factory_t default_pyr_track_factory;

kpt_tracker::kpt_tracker(int width, int height, int levels, int max_motion, bool glContext,
                    pyr_keypoint::pyr_keypoint_factory_t *kf,
                    pyr_frame::pyr_frame_factory_t *ff,
                    pyr_track::pyr_track_factory_t *tf )
	: tracks((kf?kf:&default_pyr_keypoint_factory),(ff?ff:&default_pyr_frame_factory),(tf?tf:&default_pyr_track_factory)), 
	nb_points(0), max_motion(max_motion), nb_levels(levels),
	kpt_recycler(kf?kf:&default_pyr_keypoint_factory)
{
	license();
        patch_size= patch_tagger::patch_size;
        //patch_size= 8;
	ncc_threshold=.88f;
	ncc_threshold_high=.9f;
	tree=0;
	id_clusters=0;
	pipeline_stage1=0;

#ifdef WITH_YAPE
	detector = new pyr_yape(width, height, levels); 
	detector->set_radius(5);
	points = new keypoint[10000];
#endif
}

bool kpt_tracker::load_tree(sqlite3 *db)
{
	if (db==0) return false;

	// load tree first.
	tree= kmean_tree::load(db);
	if (!tree) {
		std::cout << "Failed to load tree from database.\n";
		return false;
	}
	return true;
}

bool kpt_tracker::load_clusters(sqlite3 *db)
{
	if (db==0) return false;

	if (id_clusters) 
		delete id_clusters;
	id_clusters = new id_cluster_collection( id_cluster_collection::QUERY_NORMALIZED_FREQ );
	return id_clusters->load(db);
}

bool kpt_tracker::load_from_db(const char *dbfile)
{
	sqlite3 *db;
	int rc = sqlite3_open_v2(dbfile, &db, SQLITE_OPEN_READONLY, 0);
	if (rc) return false;
	bool r = load_from_db(db);
	sqlite3_close(db);
	return r;
}

bool kpt_tracker::load_from_db(sqlite3 *db)
{
	return load_tree(db) && load_clusters(db);
}

bool kpt_tracker::load_tree(const char *fn)
{
	tree= kmean_tree::load(fn);
	if (!tree) 
		std::cout << fn << ": failed to load tree.\n";
	
	return tree!=0;
}

bool kpt_tracker::load_clusters(const char *fn)
{
	if (id_clusters) 
		delete id_clusters;
	id_clusters = new id_cluster_collection( id_cluster_collection::QUERY_NORMALIZED_FREQ );
	if (!id_clusters->load(fn)) {
		std::cerr << "clusters.bin: failed to load id clusters.\n";
		delete id_clusters;
		id_clusters=0;
		return false;
	}
	return true;
}

void kpt_tracker::buildPyramid(pyr_frame *frame) {
	frame->pyr->build();
        cv::Mat im((*frame->pyr)[0]);
        int levels = cv::buildOpticalFlowPyramid(im, frame->cv_pyramid,
                                                 cv::Size(patch_tagger::patch_size,
                                                          patch_tagger::patch_size), 4);
}

void kpt_tracker::set_size(int width, int height, int levels, int )
{
	nb_levels = levels;
#ifdef WITH_YAPE
	delete detector;
	detector = new pyr_yape(width,height,levels);
#endif
}

pyr_frame *kpt_tracker::process_frame_pipeline(IplImage *im, long long timestamp) {
	pyr_frame *new_f=0;
	if (im) new_f = create_frame(im, timestamp);
#pragma omp parallel 
	{
#pragma omp master
	{
	if (im) {
		//TaskTimer::pushTask("pyramid");
                buildPyramid(new_f);

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
		track_ncclk(pipeline_stage1, (pyr_frame *)get_nth_frame(1));
	}
	}
	}
	pyr_frame *out = pipeline_stage1;
	pipeline_stage1 = new_f;
	return out;
}

pyr_frame *kpt_tracker::process_frame(IplImage *im, long long timestamp) {
	pyr_frame *f = create_frame(im, timestamp);
	TaskTimer::pushTask("pyramid");
        buildPyramid(f);
	TaskTimer::popTask();
	TaskTimer::pushTask("Feature detection");
	detect_keypoints(f);
	TaskTimer::popTask();
	traverse_tree(f);
	f->append_to(*this);
	track_ncclk(f, (pyr_frame *)get_nth_frame(1));
	return f;
}

pyr_frame::pyr_frame(PyrImage *p, int bits) : 
		tframe(p->images[0]->width, p->images[0]->height, bits), 
		pyr(p), tracker(0) 
{
}

void pyr_frame::append_to(tracks &t)
{
	tframe::append_to(t);
	tracker=static_cast<kpt_tracker *>(&t);
}

pyr_frame *kpt_tracker::add_frame(IplImage *im, long long timestamp) 
{
	pyr_frame *f = create_frame(im, timestamp);
        buildPyramid(f);
	f->append_to(*this);
	return f;
}

pyr_frame *kpt_tracker::create_frame(IplImage *im, long long timestamp) 
{

	// builds the pyramid and prepare memory
	pyr_frame *lf = (pyr_frame *) get_nth_frame(3);
	PyrImage *p;
	if (lf && lf->pyr) {
		p = lf->pyr;
		lf->pyr=0;
		cvReleaseImage(&p->images[0]);
		p->images[0] = im;
		lf->cv_pyramid.clear();
	} else {
		p = new PyrImage(im, nb_levels, false);
	}

	//return new pyr_frame(*this, p, 4);
	pyr_frame *f = (pyr_frame*)(((pyr_frame::pyr_frame_factory_t *) tframe_factory)->create(p,4));
	f->timestamp = timestamp;
	return f;
}


void kpt_tracker::detect_keypoints(pyr_frame *f) {

	f->tracker = this;
#ifdef WITH_YAPE
	// detects keypoints on input image pyramid
	TaskTimer::pushTask("yape");
	nb_points = detector->detect(f->pyr, points, 800);
	TaskTimer::popTask();

	TaskTimer::pushTask("descriptor");
	// transfer points to tracks structure
	for (int i=0; i<nb_points; i++) {
		//if (points[i].score>0) {
			pyr_keypoint *p = kpt_recycler.get_new();
                        p->set(f,points[i],patch_size);

			if (p->stdev < 10*10) {
				p->dispose();
			}
                        else {
                            assert(p->descriptor.total>0);
                        }

			//pyr_keypoint *p = new pyr_keypoint(f, points[i], patch_size);
		//}
	}
	TaskTimer::popTask();
#endif

#ifdef WITH_FAST
	for (int l=1; l<f->pyr->nbLev; ++l) {
		IplImage *im = f->pyr->images[l];
		int npts=1000;
		xy *pts = fast11_detect_nonmax((byte *)im->imageData, im->width, im->height, im->widthStep, 10, &npts);

		for (int i=0;i<npts; i++) {
			pyr_keypoint *p = kpt_recycler.get_new();
			p->set(f, pts[i].x, pts[i].y, l, patch_size);
			if (p->stdev < 10*10) p->dispose();
		}
		free(pts);
	}
#endif

#ifdef WITH_MSER
	cv::Mat _f(f->pyr->images[1]);
	int npts = mser.mser(_f, cv::Mat());
	for (MSERDetector::iterator i = mser.begin(); --npts>=0; ++i)
	{
		pyr_keypoint *p = kpt_recycler.get_new();
		MSERDetector::Contour contour(*i);
		mser.getRegion(contour, p->mser);
		
		float mag=2;
		p->mser.c.x *= 2; p->mser.c.y *= 2;
		p->mser.e1.x *= mag*2; p->mser.e1.y *= mag*2;
		p->mser.e2.x *= mag*2; p->mser.e2.y *= mag*2;
		
		p->set(f, p->mser.c.x, p->mser.c.y, /*log2f(cv::norm(p->mser.e1))*/ 0, patch_size);
		if (p->descriptor.total==0) p->dispose();
	}
#endif

#ifdef WITH_ADAPT_THRESH
	for (int l=0; l<f->pyr->nbLev; ++l) {
		IplImage *im = f->pyr->images[l];
		int npts= detectAdaptiveTresholdKeypoints(im, adapt_thresh_mat);

		float *pts = &CV_MAT_ELEM(*adapt_thresh_mat, float, 0, 0);
		for (int i=0;i<npts; i++) {
			pyr_keypoint *p = kpt_recycler.get_new();
			p->set(f, pts[i*2], pts[i*2+1], l, patch_size);
			if (p->stdev < 10*10) p->dispose();
		}
	}
#endif

#ifdef WITH_GOOD_FEATURES_TO_TRACK

	PyrImage eig(cvCreateImage(cvGetSize(f->pyr->images[0]), IPL_DEPTH_32F, 1),  f->pyr->nbLev);
	PyrImage tmp(cvCreateImage(cvGetSize(f->pyr->images[0]), IPL_DEPTH_32F, 1),  f->pyr->nbLev);
	CvPoint2D32f corners[2048];
	int total=0;
	int r = 1;
	for (int l=f->pyr->nbLev-1; l>=0; --l, r*=2) {
		IplImage *im = f->pyr->images[l];
		int cnt=2048;
		cvGoodFeaturesToTrack(im, eig[l], tmp[l], corners, &cnt, .01, r);
		cvFindCornerSubPix(im, corners, cnt, cvSize(5,5), cvSize(-1,-1),
				cvTermCriteria(CV_TERMCRIT_ITER| CV_TERMCRIT_EPS, 3, 0.01));
		for (int i=0; i<cnt; i++) {
			if (!f->has_point_in(corners[i].x, corners[i].y, r)) {
				pyr_keypoint *p = kpt_recycler.get_new();
				p->set(f, corners[i].x, corners[i].y, l, patch_size);
				if (total++ > 1500) break;
				
			}
		}
	}
#endif

}

bool should_track_point(pyr_keypoint *point) {
    if (point->matches.next != 0) {
        // the point is already matched.
        return false;
    }

    // If the point was matched and verified, we keep it.
    // If not, or if no verification code is used, we do not track the point.
    if (point->should_track()) {
        return true;
    }

    int length = point->track_length();
	if (length > 2 && ((pyr_track*)point->track)->nb_lk_tracked - 2 <= length/2) {
        // The track looks promising enough.
        return true;
    }

    return false;
}

void kpt_tracker::track_ncclk(pyr_frame *f, pyr_frame *lf)
{
	// match all points of frame t-1 with points on frame t
	ncc_calls=0;
	if (!f || !lf) return;
	assert(f->tracker == this);
	assert(lf->tracker == this);

	TaskTimer::pushTask("Feature tracking");
	TaskTimer::pushTask("NCC frame-to-frame matching");

	int nmatches=0;
	for (keypoint_frame_iterator it(lf->points.begin()); !it.end(); ++it) {
		pyr_keypoint *kpt = (pyr_keypoint *) it.elem();
		float ku = kpt->u;
		float kv = kpt->v;
		if (kpt->matches.prev) {
			// prediction
			ku = (ku - kpt->matches.prev->u) + ku;
			kv = (kv - kpt->matches.prev->v) + kv;
		}
		pyr_keypoint *r = best_match(kpt, f->points.search(ku, kv, float(max_motion)));
		if (r) {
			if (!r->matches.prev) {
				nmatches++;
				set_match(kpt, r);
			} else {
				// cancel match..
				unset_match(r);
			}
		}
	}


	// reverse, points of frame t with points of frame t-1
	if (0) 
	for (keypoint_frame_iterator it(f->points.begin()); !it.end(); ++it) 
	{
		pyr_keypoint *k = (pyr_keypoint *) it.elem();
		pyr_keypoint *r = best_match(k, lf->points.search(k->u, k->v, float(max_motion)));
		if (r) {
			if (r->matches.next != k) {
				// hum false alarm.. unlink.
				if (r->matches.next) {
					unset_match(r->matches.next);
				}
				if (k->matches.prev) {
					unset_match(k);
				}
			}
		}
	}

	TaskTimer::popTask();
	TaskTimer::pushTask("LK tracking");

        std::vector<cv::Point2f> prev_ft;
        std::vector<cv::Point2f> curr_ft;
        std::vector<unsigned char> lk_status;
        std::vector<pyr_keypoint *> prev_kpt;
        std::vector<float> lk_error;
        prev_ft.reserve(512);
        curr_ft.reserve(512);
        lk_status.reserve(512);
        prev_kpt.reserve(512);

	// try to track lost keypoints using template matching
	for (keypoint_frame_iterator it(lf->points.begin()); !it.end(); ++it) {
		pyr_keypoint *k = (pyr_keypoint *) it.elem();
		// a track was lost on frame t-1..
		if (k->matches.next==0 && should_track_point(k)) {
			float ku = k->u;
			float kv = k->v;
			prev_ft.push_back(cv::Point2f(ku, kv));

			// prediction
			curr_ft.push_back(cv::Point2f(ku - k->matches.prev->u + ku,
                                                    kv - k->matches.prev->v + kv)); 
			prev_kpt.push_back(k);
		}
	}
        int nft = prev_ft.size();

        if (nft > 0) {
            cv::calcOpticalFlowPyrLK(lf->cv_pyramid, f->cv_pyramid,
                                     prev_ft, curr_ft, lk_status, lk_error,
                                     cv::Size(patch_size, patch_size),
                                     4,
                                     cv::TermCriteria(cv::TermCriteria::COUNT
                                                      + cv::TermCriteria::EPS, 5, 0.1),
                                     cv::OPTFLOW_USE_INITIAL_FLOW
                                    );
        }

	int nsaved=0;

	int num_lk_failed = 0;
	int num_important_lk_failed = 0;
	for (int i=0; i<nft; i++) {
		if (!lk_status[i]) {
			num_lk_failed++;
			if (prev_kpt[i]->should_track()) {
				num_important_lk_failed++;
			}
			continue;
		}
		//std::cout << "Motion: " << prev_ft[i].x << "," << prev_ft[i].y << " -> "
		//	<< curr_ft[i].x << ", " << curr_ft[i].y << std::endl;
                pyr_keypoint *closest = (pyr_keypoint *) f->points.closest_point(curr_ft[i].x, curr_ft[i].y, 1);
		if (closest && closest->matches.prev==0) {
              set_match(prev_kpt[i], closest);
              static_cast<pyr_track *>(closest->track)->nb_lk_tracked++;
		} else {
			pyr_keypoint *newkpt = kpt_recycler.get_new();
			float s = 1.0f/(1<<(int)prev_kpt[i]->scale);
			newkpt->set(f,curr_ft[i].x*s, curr_ft[i].y*s, prev_kpt[i]->scale, patch_size);
			newkpt->score = prev_kpt[i]->score;
            if (1 || cmp_ncc(prev_kpt[i], newkpt)> ncc_threshold) {
				set_match(prev_kpt[i],newkpt);
                                static_cast<pyr_track *>(newkpt->track)->nb_lk_tracked++;
                                nsaved++;
			} else {
				newkpt->dispose();
			}
		}
	}

	//if (f->points.size()>0)
	//cout << ncc_calls << " calls to cmp_ncc, for " << f->points.size() << " points in frame. Avg: " 
	//	<< ncc_calls / f->points.size() ;
	
	if (0) {
		int num_lost = 0;
		for (keypoint_frame_iterator it(lf->points.begin()); !it.end(); ++it) {
			pyr_keypoint *kpt = (pyr_keypoint *) it.elem();
			if (kpt->should_track() && kpt->matches.next == 0) {
				++num_lost;
			}
		}
		cout << "tracking lost " << num_lost << " important keypoints. "
			<< "num_lk_failed: " << num_lk_failed << " "
			<< "num_important_lk_failed: " << num_important_lk_failed << endl;
	}

	if (0) {
		cout << nmatches << " ncc matches";
		cout << ", tracked " << nft << " features with LK. Saved " << nsaved << " tracks. ";
		cout << "tot: " << nmatches + nsaved << " features followed. " << (100.0f*nsaved/(nsaved+nmatches)) << "% LK tracked.\n";
	}
	
	TaskTimer::popTask();
	TaskTimer::popTask();

}

void pyr_keypoint::prepare_patch(int win_size)
{
	TaskTimer::pushTask("descriptor");
	int half = win_size/2;
	win_size |= 1;

        pyr_frame *f = static_cast<pyr_frame *>(frame);
        //int s = std::min(f->pyr->nbLev-1, scale+1);
        int s = scale;
        IplImage *im = f->pyr->images[s];

        float level_u = (s==scale? level.u : level.u/2);
        float level_v = (s==scale? level.v : level.v/2);
        CvRect rect = cvRect(level_u-half, level_v-half, win_size, win_size);


	if (((rect.x | rect.y) < 0) || (rect.x+rect.width> im->width) || (rect.y+rect.height> im->height)) 
	//if (1)
	{
		int aw = (win_size+3) & 0xfffffc;
		if (!data) data = new unsigned char[aw*aw];
		cvInitMatHeader(&patch, win_size, win_size, CV_8UC1, data, aw);

		for (int j= -half; j<=half; j++) {
			int y= level_v+j;
			if (y<0) y=0;
			if (y>=im->height) y=im->height-1;
			unsigned char *d = data+(j+half)*aw;

			for (int i= -half; i<=half; i++) {

				int x= level_u+i;
				if (x<0) x=0;
				if (x>=im->width) x=im->width-1;
					
				*d++ = CV_IMAGE_ELEM(im, unsigned char, y, x);
			}
		}
	} else {
		cvGetSubRect(im, &patch, rect);
		if (data) delete[] data;
		data=0;
	}

	unsigned sum=0;
	unsigned sqsum=0;
	// computes mean and variance
	for (int j=0; j<patch.rows; j++) {
		unsigned char *p = &CV_MAT_ELEM(patch, unsigned char, j, 0);
		for (int i=0; i<patch.cols; i++) {
			unsigned f = *p++;
			sum += f;
			sqsum += f*f;
		}
	}
	float n = (patch.rows*patch.cols);
	mean = sum/n;
	stdev = sqrtf(sqsum - n*mean*mean);

	if (stdev < .1) {
		id=0;
		cid=0;
		descriptor.total=0;
		stdev=0;
		TaskTimer::popTask();
		return;
	}


	id = 0;
	cid=0;

#ifdef WITH_PATCH_TAGGER_DESCRIPTOR
	float subpix_x = level_u - floor(level_u);
        float subpix_y = level_v - floor(level_v);

	patch_tagger::singleton()->cmp_orientation(&patch, &descriptor);

	cv::Size size(patch_tagger::patch_size, patch_tagger::patch_size);
	cv::Mat rotated(size, CV_32FC1, descriptor._rotated);
	ExtractPatch(*this, size, &rotated);

	if (descriptor.total ==0) {
		stdev=0;
	}
#endif
	TaskTimer::popTask();
}

void kpt_tracker::traverse_tree(pyr_frame *frame)
{
	if (!tree || !frame) return ;

	TaskTimer::pushTask("tree");

	for (tracks::keypoint_frame_iterator it(frame->points.begin()); !it.end(); ++it) {
		pyr_keypoint *k = (pyr_keypoint *) it.elem();
#ifdef WITH_PATCH_TAGGER_DESCRIPTOR
		kmean_tree::descriptor_t array;
		k->descriptor.array(array.descriptor);
		k->id = tree->get_id(&array, &k->node);
#endif
	}
	TaskTimer::popTask();
}

/*
pyr_keypoint::pyr_keypoint(tframe *f, keypoint &pt, int patch_size) 
		: tkeypoint(f,pt.fr_u(),pt.fr_v()),
		scale(pt.scale), level(pt.u,pt.v)
{
	data=0; 
	prepare_patch(patch_size);
}

pyr_keypoint::pyr_keypoint(tframe *f, float u, float v, int scale, int patch_size) 
		: tkeypoint(f,u*(1<<scale),v*(1<<scale)), scale(scale), level(u,v)
{
	data=0; 
	prepare_patch(patch_size);
}
*/
pyr_keypoint::pyr_keypoint(const pyr_keypoint &a)
	: tkeypoint(a), scale(a.scale), level(a.level), mean(a.mean),
	stdev(a.stdev), id(a.id), cid(a.cid),
	descriptor(a.descriptor),
	node(a.node)
{
	patch = a.patch;
	if (a.data) {
		int sz = patch.step * patch.rows;
		data = new unsigned char [sz];
		memcpy(data,a.data,sz);
		patch.data.ptr = data;
	} else {
		data=0;
	}
}

const pyr_keypoint &pyr_keypoint::operator = (const pyr_keypoint &a)
{
	tkeypoint::operator =(a);
	scale=a.scale;
	level=a.level;
	mean=a.mean;
	stdev=a.stdev;
	id=a.id;
	cid=a.cid;
	descriptor=a.descriptor;
	node=a.node;
	assert(data==0 || a.data==0 || a.patch.rows == patch.rows);
	patch = a.patch;
	if (a.data) {
		int sz = patch.step * patch.rows;
		if (data==0) data = new unsigned char [sz];
		memcpy(data,a.data,sz);
		patch.data.ptr = data;
	} else if (data) {
		delete[] data;
		data=0;
	}
	return *this;
}


void pyr_keypoint::set(tframe *f, keypoint &pt, int patch_size) 
{
	tkeypoint::set(f,pt.fr_u(), pt.fr_v());
	if (data) delete[] data;
	data=0; 
	scale=pt.scale;
	score = pt.score;
	level.u = pt.u;
	level.v = pt.v;
	prepare_patch(patch_size);
	node=0;
}
void pyr_keypoint::set(tframe *f, float u, float v, int scale, int patch_size) 
{
	tkeypoint::set(f,u*(1<<scale),v*(1<<scale));
	if (data) delete[] data;
	data=0; 
	score=0;
	this->scale = std::min(scale, ((pyr_frame *)f)->pyr->nbLev-1);
	level.u = u;
	level.v = v;
	prepare_patch(patch_size);
	node=0;
}

pyr_keypoint::~pyr_keypoint() {
	if (data) delete[] data;
	data=0;
}

#ifdef WITH_MSER
bool pyr_keypoint::get_mser_patch(PyrImage &pyr, cv::Mat dst)
{
	unsigned w = dst.cols;
	unsigned h = dst.rows;
	unsigned scale = std::min(unsigned(pyr.nbLev-1),(unsigned)max(0.0f,floorf(log2f(cv::norm(mser.e2)/w)+1)));
	float s(1.0f/(1 << scale));

	float _a[] = {
		s*mser.e1.x*2.0f/w, s*mser.e2.x*2.0f/h, s*mser.c.x,
		s*mser.e1.y*2.0f/w, s*mser.e2.y*2.0f/h, s*mser.c.y
	};
	//cv::Mat A(2,3, CV_32FC1, _a);
	CvMat A; 
	cvInitMatHeader(&A, 2, 3, CV_32FC1, _a);

	CvMat _dst = dst;
	cvGetQuadrangleSubPix(pyr[scale], &_dst, &A);

	CvScalar mean, stdev;

	cvAvgSdv(&_dst, &mean, &stdev);

	if (stdev.val[0] < .0005) return false;

	double div_sdv(.5/stdev.val[0]);
	dst = dst*div_sdv-mean.val[0]*div_sdv + .5;
	return true;
}
#endif



float cmp_ncc(pyr_keypoint *a, pyr_keypoint *b)
{
	ncc_calls++;


	int w=a->patch.cols;
	int h=a->patch.rows;

	int ma = a->mean;
	int mb = b->mean;
	int sum=0;
	float norm = (a->stdev*b->stdev);
	if (a->stdev < 1 || b->stdev<1) return 0;

	if (a->id && a->id == b->id) return .95;

	//float threshold = .2*norm;
	for (int j=0; j<h; j++) {
		const unsigned char *pa = &CV_MAT_ELEM(a->patch, unsigned char, j, 0);
		const unsigned char *pb = &CV_MAT_ELEM(b->patch, unsigned char, j, 0);
		int i=w;
		do {
			sum += ((*pa++)-ma)*((*pb++)-mb);
		} while(--i);

		//if (j>(h>>1)) if (sum<threshold) return 0;
	}

	return sum/norm;
}


struct score_kpt {
	int score;
	pyr_keypoint *k;
};
bool operator <(score_kpt &a, score_kpt &b) {
	return a.score < b.score;
}
int score_kpt_cmp(const void *_a, const void *_b) {
	score_kpt *a = (score_kpt*)_a;
	score_kpt *b = (score_kpt*)_b;
	return (a->score < b->score ? 0 : 1);
}

pyr_keypoint *kpt_tracker::best_match(pyr_keypoint *templ, tracks::keypoint_frame_iterator it)
{
	if (it.end()) return 0;

	pyr_keypoint *best_kpt=0;
	float best_corr=0;


	for ( ; !it.end(); ++it) {
		pyr_keypoint *k = (pyr_keypoint *) it.elem();
		float ncc = cmp_ncc(templ, k);
		if (ncc> best_corr) {
			best_corr = ncc;
			best_kpt = k;
			if (best_corr>ncc_threshold_high) break;
		}
	}

	if (best_corr>ncc_threshold) return best_kpt;
	return 0;
}

pyr_frame::~pyr_frame() {
	if (pyr) delete pyr;
}

#if 0
void kpt_tracker::match_template(pyr_keypoint *templ, pyr_frame *frame, float delta[2])
{

	point2d pos(templ->u,templ->v);

	//delta[0]=delta[1]=0;

	if (!templ || !frame || !frame->pyr || !frame->pyr->images[0]) return;


		int jacobian[PATCH_SIZE][PATCH_SIZE][2];
		float pseudo_inv[2][2];

		// compute patch derivatives and pseudo-inverse
		for (int y=0; y<templ->patch.rows-1; y++) {
			unsigned char *p = &CV_MAT_ELEM(templ->patch, unsigned char, y, 0);
			for (int x=0; x<templ->patch.cols-1; x++) {
				jacobian[y][x][0] = p[1]-p[0];
				jacobian[y][x][1] = p[templ->patch.step]-p[0];
			}
		}
		{
			int x = templ->patch.cols-1;
			unsigned char *p = &CV_MAT_ELEM(templ->patch, unsigned char, 0, x);
			for (int y=0; y<templ->patch.rows; y++) {
				jacobian[y][x][0] = jacobian[y][x-1][0];



		CvRect r = cvRect(0,0, templ->patch.cols, templ->patch.rows);
		int half = templ->patch.cols/2;

		// iterate
		for (int iter=0; iter<10; iter++) {
			CvMat tg;
			r.x = pos.u - half;
			r.y = pos.v - half;
			cvGetSubRect(frame->pyr->images[level],&tg, r);

			float JtD[2] = {0,0};
			for (int y=0;y<r.height;y++) {
				unsigned char *tp = &CV_MAT_ELEM(templ->patch, unsigned char, y, 0);
				unsigned char *ip = &CV_MAT_ELEM(tg, unsigned char, y, 0);
				for (int x=0; x<r.width; x++) {
					int err = ip[x] - tp[x];
					JtD[0] += jacobian[y][x][0]*err;
					JtD[1] += jacobian[y][x][1]*err;
				}
			}
			pos.u -= pseudo_inv[0][0]*JtD[0] + pseudo_inv[0][1]*JtD[1];
			pos.v -= pseudo_inv[1][0]*JtD[0] + pseudo_inv[1][1]*JtD[1];
		}
}
#endif

pyr_track::pyr_track(tracks *db) : ttrack(db), id_histo(0), nb_lk_tracked(0) {}

void pyr_track::point_added(tkeypoint *p)
{
	ttrack::point_added(p);

	pyr_keypoint *pk = (pyr_keypoint *) p;
	pyr_frame *f = (pyr_frame *)pk->frame;
	id_histo.database = f->tracker->id_clusters;
	if (pk->id) {
		TaskTimer::pushTask("Cluster retrieval");
	       	id_histo.modify(pk->id,1);
	//if (pk->id && pk->track_is_longer(4)) {

		if (f->tracker->id_clusters && id_histo.query_cluster.total>0) {
			incremental_query::iterator it = id_histo.sort_results_min_ratio(.98f);
			//incremental_query::iterator it = id_histo.sort_results(2);
			if (it != id_histo.end()) {
				pk->cid = it->c->id;
				pk->cscore = it->score;
			} else 
				pk->cid=0;
		}
		TaskTimer::popTask();
	} else {
		pk->cid=0;
	}
}

void pyr_track::point_removed(tkeypoint *p)
{
	ttrack::point_removed(p);
	pyr_keypoint *pk = (pyr_keypoint *) p;
	if (pk->id) id_histo.modify(pk->id, -1);
}
