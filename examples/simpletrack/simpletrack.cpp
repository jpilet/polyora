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
/*! \file simpletrack.cpp
 * This example demonstrates how to track points in a sequence of images using polyora.
 *
 * \author Julien Pilet, 2009-2010.
 */

#include <iostream>
#include <highgui.h>

#include <polyora/polyora.h>

using namespace std;

int main(int argc, char *argv[]) {

	if (argc<2) {
		cerr << "usage: " << argv[0] << " <image> [<image> ...]\n";
		return -1;
	}

	// load an image to obtain the width/height
	IplImage *first = cvLoadImage(argv[1]);
	if (first==0) {
		cerr << argv[1] << ": can't load image\n";
		return -1;
	}
	int width = first->width;
	int height = first->height;
	cvReleaseImage(&first);

	kpt_tracker tracker(width, height, 3, 10);

	// Optionally loads quantization tree and track clusters
	tracker.load_from_db("visual.db");

	for (int i=1; i<argc; i++) {
		// the 0 is here to tell opencv to convert the image to grey level,
		// because process_frame accepts only single channel images
		IplImage *im = cvLoadImage(argv[i], 0);
		if (!im) {
			cerr << argv[i] << ": can't load image\n";
			continue;
		}

		// process_frame will detect points and take care of calling cvReleaseImage(im) when required.
		tracker.process_frame(im, i);

		// get a pointer to the most recent frame
		pyr_frame *frame= (pyr_frame *)tracker.get_nth_frame(0);

		// iterate over all the frame's keypoints
		for (tracks::keypoint_frame_iterator it(frame->points.begin()); !it.end(); ++it) {
			pyr_keypoint *k = (pyr_keypoint *) it.elem();

			cout << "(" << k->u << "," << k->v << ")";
		}
		cout << endl;

		// free tracks lost two frames ago
		tracker.remove_unmatched_tracks(tracker.get_nth_frame(2));
	}

	return 0;
}
