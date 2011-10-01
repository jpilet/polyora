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
#include <highgui.h>
#include "pic_randomizer.h"
#include <iostream>

#ifndef WIN32
#include <sys/file.h>
#else
#include <float.h>
static inline double drand48() {
	return (double)rand()/(double)RAND_MAX;
}

static inline int finite(float f) {
	return _finite(f);
}
#endif


#ifdef _OPENMP
#include <omp.h>
#endif

#ifndef M_PI
#define M_PI CV_PI
#endif

using namespace std;

#ifdef _OPENMP
static omp_lock_t lock;
bool lock_initialized=false;
#endif

FILE *open_and_lock(const char *fn)
{
#ifdef _OPENMP
	if (!lock_initialized) {
		omp_init_lock(&lock);
		lock_initialized = true;
	}
	omp_set_lock(&lock);
#endif
	FILE *ofile = fopen(fn,"a");
	if (!ofile) return 0;

#ifndef WIN32
	if (flock(fileno(ofile), LOCK_EX) < 0) {
		fprintf(stderr, "locking ");
		perror(fn);
		fclose(ofile);
		return 0;
	}
#endif
	return ofile;
}

void unlock_and_close(FILE *f)
{
#ifndef WIN32
	flock(fileno(f), LOCK_UN);
#endif
	fclose(f);

#ifdef _OPENMP
	omp_unset_lock(&lock);
#endif
}


pic_randomizer::pic_randomizer()
{
	base_image = 0;
	view = 0;
	points = 0;
	tracker=0;
	save_descriptors_only= false;
	tree_fn = 0;
	output_fn = 0;
	visualdb_fn = 0;

	min_view_rate = .3;
	nb_views = 1000;
}

pic_randomizer::~pic_randomizer() {
        if (base_image) cvReleaseImage(&base_image);
        if (view) cvReleaseImage(&view);
        if (points) delete points;
        if (tracker) delete tracker;
}


bool pic_randomizer::load_image(const char *fn){
	cvReleaseImage(&base_image);
	cvReleaseImage(&view);
	if (points) delete points;
	points=0;

	base_image = cvLoadImage(fn, 0);
	if (!base_image) return false;

	while (base_image->width>1024) {
		IplImage *smaller = cvCreateImage(cvSize(base_image->width/2,base_image->height/2),
				IPL_DEPTH_8U, 1);
		cvPyrDown(base_image, smaller);
		cvReleaseImage(&base_image);
		base_image = smaller;
	}

	points = new bucket2d<point_cluster>(base_image->width, base_image->height, 4);

	view=0;
	return true;
}

float rand_range(float min, float max)
{
	if (min>max) {
		float t = min;
		min = max;
		max = t;
	}
	return drand48()*(max-min)+min;
}

void Corners::interpolate(const Corners &a, const Corners &b, float t)
{
	float tm = 1.0f - t;
	for (int i=0; i<4; i++) {
		p[i][0] = a.p[i][0]*t + a.p[i][0]*tm;
		p[i][1] = a.p[i][1]*t + a.p[i][1]*tm;
	}
}

void Corners::random_homography(float src_width, float src_height, float min_angle, float max_angle, float min_scale, float max_scale, float h_range, float v_range)
{
	float angle = rand_range(min_angle, max_angle);
	float scale = rand_range(min_scale, max_scale);
	
	p[0][0] = rand_range(0,h_range); p[0][1] = rand_range(0,v_range); 
	p[1][0] = rand_range(src_width-h_range, src_width); p[1][1] = rand_range(0,v_range); 
	p[2][0] = rand_range(src_width-h_range, src_width); p[2][1] = rand_range(src_height-v_range, src_height); 
	p[3][0] = rand_range(0,h_range); p[3][1] = rand_range(src_height-v_range, src_height);

	float center[2] = {0,0};
	for (int i=0; i<4; i++) {
		center[0] += p[i][0]/4;
		center[1] += p[i][1]/4;
	}
	for (int i=0; i<4; i++) {
		p[i][0] -= center[0];
		p[i][1] -= center[1];
	}

	// scaling
	for (int i=0; i<4; i++) {
		p[i][0] *= scale;
		p[i][1] *= scale;
	}

	// rotation
	float rot[4][2];
	for (int i=0; i<4; i++) {
		float a = cos(angle)*p[i][0] - sin(angle)*p[i][1];
		float b = sin(angle)*p[i][0] + cos(angle)*p[i][1];
		p[i][0] = a;
		p[i][1] = b;
	}
}

void Corners::get_min_max(float *minx, float *miny, float *maxx, float *maxy)
{
	// find min-max
	float xmin=1e8,xmax=-1e8;
	float ymin=1e8,ymax=-1e8;
	for (int i=0; i<4; i++) {
		if (p[i][0] < xmin) xmin = p[i][0];
		if (p[i][0] > xmax) xmax = p[i][0];
		if (p[i][1] < ymin) ymin = p[i][1];
		if (p[i][1] > ymax) ymax = p[i][1];
	}
	if (minx) *minx = xmin;
	if (miny) *miny = ymin;
	if (maxx) *maxx = xmax;
	if (maxy) *maxy = ymax;
}


void Corners::fit_in_image(int *width, int *height)
{
	float xmin,ymin;
	float xmax,ymax;
	get_min_max(&xmin, &ymin,&xmax,&ymax);
	for (int i=0;i<4; i++) {
		p[i][0] -= xmin;
		p[i][1] -= ymin;
	}
	if (width)
		*width = ((int)(xmax - xmin) + 7) & 0xFFFFFFF8;
	if (height)
		*height = ((int)(ymax - ymin) + 7) & 0xFFFFFFF8;
}

void Corners::get_homography(CvMat *H, int src_width, int src_height)
{
	float src_pts[4][2] = {
		{0,0},
		{src_width,0},
		{src_width,src_height},
		{0,src_height}
	};

	CvMat msrc; cvInitMatHeader(&msrc, 4, 2, CV_32FC1, src_pts);
	CvMat mdst; cvInitMatHeader(&mdst, 4, 2, CV_32FC1, p);
	cvFindHomography(&mdst, &msrc, H);
}

void SyntheticViewPath::generatePath(int nbLoc, float min_angle, float max_angle, float min_scale, float max_scale, float h_range, float v_range)
{
	path.clear();
	if (nbLoc<2) return;
	path.reserve(nbLoc);
	for (int i=0; i<nbLoc; i++) {
		path.push_back(Corners()); 
		path[i].random_homography(im->width, im->height, min_angle, max_angle, min_scale, max_scale, h_range, v_range);
	}
}

void SyntheticViewPath::getDstImSize(int *width, int *height)
{
	int max_w=0, max_h=0; 
	for (int i=0; i<path.size(); i++) {
		int w,h;
		path[i].fit_in_image(&w,&h);
		if (w>max_w) max_w=w;
		if (h>max_h) max_h=h;
	}
	*width = max_w;
	*height = max_h;
}

void SyntheticViewPath::genView(float _t, IplImage *dst)
{

	float t = _t;
	if (_t<0) t=0;
	if (_t>=1) t=1;
	float n = t*path.size();
	int pos = floorf(n);
	float sub = n-floorf(n);
	if (pos == path.size()) {
		--pos;
		t+=1;
	}
	Corners c;
	c.interpolate(path[pos], path[pos+1], t);

	CvMat H;
	float _H[3][3];
	cvInitMatHeader(&H, 3, 3, CV_32FC1, _H);

	c.get_homography(&H, im->width, im->height);

	cvWarpPerspective(im, dst, &H,  CV_WARP_FILL_OUTLIERS+ CV_WARP_INVERSE_MAP);

}

void random_homography(CvMat *H, int *width, int *height, float min_angle, float max_angle, float min_scale, float max_scale, float h_range, float v_range)
{
	float angle = rand_range(min_angle, max_angle);
	float scale = rand_range(min_scale, max_scale);
	
	float pts[4][2]= { 
		{rand_range(0,h_range),rand_range(0,v_range)}, 
		{rand_range(*width-h_range, *width),rand_range(0,v_range)}, 
		{rand_range(*width-h_range, *width),rand_range(*height-v_range, *height)}, 
		{rand_range(0,h_range),rand_range(*height-v_range, *height)}, 
	};
	float center[2] = {0,0};
	for (int i=0; i<4; i++) {
		center[0] += pts[i][0]/4;
		center[1] += pts[i][1]/4;
	}
	for (int i=0; i<4; i++) {
		pts[i][0] -= center[0];
		pts[i][1] -= center[1];
	}

	// scaling
	for (int i=0; i<4; i++) {
		pts[i][0] *= scale;
		pts[i][1] *= scale;
	}

	// rotation
	float rot[4][2];
	for (int i=0; i<4; i++) {
		rot[i][0] = cos(angle)*pts[i][0] - sin(angle)*pts[i][1];
		rot[i][1] = sin(angle)*pts[i][0] + cos(angle)*pts[i][1];
	}

	// find min-max
	float xmin=1e8,xmax=-1e8;
	float ymin=1e8,ymax=-1e8;
	for (int i=0; i<4; i++) {
		if (rot[i][0] < xmin) xmin = rot[i][0];
		if (rot[i][0] > xmax) xmax = rot[i][0];
		if (rot[i][1] < ymin) ymin = rot[i][1];
		if (rot[i][1] > ymax) ymax = rot[i][1];
	}
	for (int i=0;i<4; i++) {
		rot[i][0] -= xmin;
		rot[i][1] -= ymin;
	}

	float src_pts[4][2] = {
		{0,0},
		{*width,0},
		{*width,*height},
		{0,*height}
	};

	*width = ((int)(xmax - xmin) + 7) & 0xFFFFFFF8;
	*height = ((int)(ymax - ymin) + 7) & 0xFFFFFFF8;

	CvMat msrc; cvInitMatHeader(&msrc, 4, 2, CV_32FC1, src_pts);
	CvMat mdst; cvInitMatHeader(&mdst, 4, 2, CV_32FC1, rot);
	cvFindHomography(&mdst, &msrc, H);
}

IplImage *pic_randomizer::gen_view()
{
	int width = base_image->width;
	int height = base_image->height;

	cvInitMatHeader(&mH, 3, 3, CV_32FC1, H);

	random_homography(&mH, &width, &height, 0, 2*M_PI, .5, 2, width*.2, height*.2);

	cvReleaseImage(&view);
	view = cvCreateImage(cvSize(width,height), base_image->depth, base_image->nChannels);
	cvWarpPerspective(base_image, view, &mH,  CV_WARP_FILL_OUTLIERS+ CV_WARP_INVERSE_MAP);

	return view;
}

pic_randomizer::point_cluster *pic_randomizer::find_cluster(const point2d &p, float r)
{
	float best_d=r;
	point_cluster *best_c=0;
	for (bucket2d<point_cluster>::iterator it(points->search(p.u, p.v, r)); !it.end(); ++it)
	{
		float d = it.elem()->dist(p);
		if (d<best_d) {
			best_c = it.elem();
			best_d = d;
		}
	}
	return best_c;
}

void pic_randomizer::detect_kpts() 
{
	if (!base_image) return;
	if (!view) gen_view();
	assert(view!=0);

	if (!tracker) {
		tracker = new kpt_tracker(view->width, view->height, 4, 10);

		if (!save_descriptors_only) {
			if (!tree_fn && !visualdb_fn) {
				cerr << "please specify a tree file or a database with -t or -v, or use the -D option.\n";
				exit(-4);
			}
			if (tree_fn && !tracker->load_tree(tree_fn)) 
			{
				cerr << tree_fn << ": can't load tree.\n";
				exit(-3);
			}
			if (visualdb_fn) {
				sqlite3 *db;
				int rc = sqlite3_open_v2(visualdb_fn, &db, SQLITE_OPEN_READONLY, 0);
				if (rc || !tracker->load_tree(db))
				{
					if (!rc) sqlite3_close(db);
					cerr << visualdb_fn << ": can't load tree.\n";
					exit(-3);
				}
				sqlite3_close(db);
			}
		}
	} else
		tracker->set_size(view->width, view->height, 4, 10);

	pyr_frame *frame = tracker->add_frame(view);
	tracker->detect_keypoints(frame);
	tracker->traverse_tree(frame);
} 

bool write_descriptor(FILE *descrf, pyr_keypoint *k, long ptr)
{
	static const unsigned descr_size=kmean_tree::descriptor_size;
	if ((sizeof(long)+descr_size*sizeof(float)) != sizeof(kmean_tree::descr_file_packet)) {
		cerr << "wrong structure size!!\n";
		exit(-1);
	}

#ifdef WITH_SURF
	if (k->surf_descriptor.descriptor[0]==-1) return false;
	for (unsigned i=0;i<64;i++) {
		if (!finite(k->surf_descriptor.descriptor[i])) {
			cerr << "surf descr[" << i << "] = " << k->surf_descriptor.descriptor[i] << endl;
			return false;
		}
	}

	fwrite(&ptr, sizeof(long), 1, descrf);
	if (fwrite(k->surf_descriptor.descriptor, 64*sizeof(float), 1, descrf)!=1) {
		cerr << "write error!\n";
		return false;
	}
#endif

#ifdef WITH_SIFTGPU
	for (unsigned i=0;i<128;i++) {
		if (!finite(k->sift_descriptor.descriptor[i])) {
			cerr << "sift descr[" << i << "] = " << k->sift_descriptor.descriptor[i] << endl;
			return false;
		}
	}

	fwrite(&ptr, sizeof(long), 1, descrf);
	if (fwrite(k->sift_descriptor.descriptor, 128*sizeof(float), 1, descrf)!=1) {
		cerr << "write error!\n";
		return false;
	}
#endif


#ifdef WITH_YAPE
	if (k->descriptor.total==0) {
		std::cout << "save_descriptors: total==0!\n";
		return false;
	}

	fwrite(&ptr, sizeof(long), 1, descrf);
	float array[descr_size];
	k->descriptor.array(array);
	fwrite(array, descr_size * sizeof(float), 1,descrf);
#endif

	return true;
}


bool pic_randomizer::save_keypoints() 
{
	FILE *ofile = open_and_lock(output_fn);
	if (!ofile) return false;

	pyr_frame *frame = (pyr_frame *)tracker->get_nth_frame(0);

	for (tracks::keypoint_frame_iterator it(frame->points.begin()); !it.end(); ++it) {
		write_descriptor(ofile, (pyr_keypoint *)it.elem(), 0);
	}
	unlock_and_close(ofile);

	tracker->remove_frame(frame);
	view=0;
	return true;
}


void pic_randomizer::group_ids() 
{
	/*
	IplImage *colIm = cvCreateImage(cvGetSize(base_image), IPL_DEPTH_8U, 3);
	cvCvtColor(base_image, colIm, CV_GRAY2BGR);
	*/

	pyr_frame *frame = (pyr_frame *)tracker->get_nth_frame(0);

	assert(tracker->tree!=0);
	for (tracks::keypoint_frame_iterator it(frame->points.begin()); !it.end(); ++it) {

		float _u = it.elem()->u;
		float _v = it.elem()->v;
		float z = H[2][0]*_u + H[2][1]*_v + H[2][2];
		float u = (H[0][0]*_u + H[0][1]*_v + H[0][2])/z;
		float v = (H[1][0]*_u + H[1][1]*_v + H[1][2])/z;
		//int iu= (int) u;
		//int iv= (int) v;
		point2d p(u,v);

		point_cluster *c = find_cluster(p, 3);
		if (!c) {
			c = new point_cluster;
			c->u = u;
			c->v = v;
			c->n=1;
			points->add_pt(c);
			//std::cout << "new keypoint at " << u << "," << v << " (detected on transformed frame at " << 
			//	_u << "," << _v << ")\n";
			
			/*
			CV_IMAGE_ELEM(colIm, unsigned char, iv, iu*3) = 0;
			CV_IMAGE_ELEM(colIm, unsigned char, iv, iu*3+1) = 0;
			CV_IMAGE_ELEM(colIm, unsigned char, iv, iu*3+2) = 255;
			*/
		} else {
			// merge with c
			c->n++;
			float nu = (u + c->u*(c->n-1))/c->n;
			float nv = (v + c->v*(c->n-1))/c->n;
			//std::cout << "merging keypoint at " << u << "," << v << " (detected on transformed frame at " << 
			//	_u << "," << _v << ", center: " << c->u << "," << c->v << " -> " << nu << "," << nv << ")\n";
			points->move_pt(c, nu, nv);
			/*
			iu = (int)nu;
			iv = (int)nv;
			CV_IMAGE_ELEM(colIm, unsigned char, iv, iu*3) = c->n*2;;
			CV_IMAGE_ELEM(colIm, unsigned char, iv, iu*3+1) = 255;
			CV_IMAGE_ELEM(colIm, unsigned char, iv, iu*3+2) = c->n*2;
			*/
		}
		c->add(((pyr_keypoint *)(it.elem()))->id, 1);
	}
	
	//char str[512];
	//sprintf(str,"view_%d.png", n_views);
	//cvSaveImage(str, colIm);
	tracker->remove_frame(frame);
	view=0;
}

void pic_randomizer::prune() 
{
	
	unsigned min = nb_views*min_view_rate;
	int removed=0;
	int remaining=0;
	for (bucket2d<point_cluster>::iterator it(*points); !it.end(); ) 
	{
		bucket2d<point_cluster>::iterator a(it);
		++it;
		if (a.elem()->total<min) {
			points->rm_pt(a.elem());
			removed++;
		} else
			remaining++;
	}
	std::cout << "Removed " << removed << " clusters with less than " << min << " views. " 
		<< points->size() << " points left (" << remaining << ")\n";
}

bool pic_randomizer::save_points()
{
	if (!points) return true;

	FILE *ofile = open_and_lock(output_fn);
	if (!ofile) return false;

	id_cluster_collection clusters(id_cluster_collection::QUERY_NORMALIZED_FREQ);
	for (bucket2d<point_cluster>::iterator it(*points); !it.end(); ++it) 
	{
		clusters.add_cluster(it.elem());
	}

	clusters.save(ofile);

	delete points;
	points=0;

	unlock_and_close(ofile);
	return true;
}

bool pic_randomizer::run() 
{
	for (int i=0; i<nb_views; i++) {
		gen_view();
		detect_kpts();
		if (save_descriptors_only) {
			if (!save_keypoints()) return false;
		} else
			group_ids();
	}
	if (!save_descriptors_only) {
		prune();
		if (!save_points()) return false;
	}
	return true;
}
