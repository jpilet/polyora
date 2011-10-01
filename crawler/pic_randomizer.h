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
#ifndef PIC_RANDOMIZER_H
#define PIC_RANDOMIZER_H

#include <cv.h>
#include <vector>
#include <stdio.h>
#include <polyora/kpttracker.h>

struct Corners {
	float p[4][2];

	void random_homography(float src_width, float src_height, float min_angle, float max_angle, float min_scale, float max_scale, float h_range, float v_range);
	void get_min_max(float *minx, float *miny, float *maxx, float *maxy);
	void fit_in_image(int *width, int *height);
	void get_homography(CvMat *H, int src_width, int src_height);

	void interpolate(const Corners &a, const Corners &b, float t);
};

class SyntheticViewPath {
public:

	SyntheticViewPath(IplImage *im) : im(im) {}
	void generatePath(int nbLoc, float min_angle, float max_angle, float min_scale, float max_scale, float h_range, float v_range);


	void genView(float t, IplImage *dst);
	void getDstImSize(int *width, int *height);

protected:
	IplImage *im;
	std::vector<Corners> path;
};


class pic_randomizer {
public:
	pic_randomizer();
	~pic_randomizer();

	bool load_image(const char *fn);

	bool run();

	IplImage *gen_view();
	void detect_kpts();
	void group_ids();
	bool save_points();
	void prune();
	bool save_keypoints();

	int nb_views;
	float min_view_rate;

	bool save_descriptors_only;

	const char *tree_fn;
	const char *output_fn;
	const char *visualdb_fn;
private:

	float H[3][3];
	CvMat mH; 
	IplImage *base_image;
	IplImage *view;

	class point_cluster : public point2d, public id_cluster {
	public:
		int n;

		mlist_elem<point_cluster> points_in_frame;
	};

	bucket2d<point_cluster> *points;

	point_cluster *find_cluster(const point2d &p, float r);

	kpt_tracker *tracker;
};

#endif
