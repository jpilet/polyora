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
/* Julien Pilet, 2009. */
#ifndef PATCHTAGGER_H
#define PATCHTAGGER_H

#include <opencv/cv.h>

class patch_tagger {
public:

        static const unsigned patch_size = 16;
	static const unsigned nb_zone = 4;
	static const unsigned nb_orient = 16;
	static const unsigned nb_test = 16;

	static patch_tagger *singleton();

	struct descriptor {
		unsigned histo[nb_zone][nb_orient];
		unsigned total;
	        unsigned sum_orient[nb_orient];	
		float orientation;
		unsigned projection;
		float _rotated[patch_size][patch_size];
		CvMat rotated;

		void array(float *array);

		descriptor();
		descriptor(const descriptor &a);
		const descriptor &operator=(const descriptor &a);
		float correl(const descriptor &a);
	};

	void cmp_descriptor(CvMat *patch, descriptor *d, float subpix_u, float subpix_v);
	unsigned project(patch_tagger::descriptor *d);
	

	void precalc();

protected:

	struct histo_entry {
		unsigned weight1;
		unsigned weight2;
		unsigned char zone1;
		unsigned char zone2;
	};

	histo_entry weight_table[patch_size][patch_size];

	struct grad2polar_entry {
		unsigned length1;
		unsigned length2;
		unsigned char dir1;
		unsigned char dir2;
	};
	grad2polar_entry cart2polar_table[512][512];


	struct random_node {
		unsigned char zone1, orient1;
		unsigned char zone2, orient2;
	};
	random_node random_tests[nb_test];

	static float _weight[patch_size][patch_size];
	CvMat weight;
	static float tot_weight;

	unsigned char _mask[patch_size][patch_size];
	CvMat mask;
	static void compute_sift_descriptor(float* descr_pt, cv::Mat patch);
};

#endif
