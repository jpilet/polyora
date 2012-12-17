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
#include <highgui.h>

#include <polyora/polyora.h>
#include <videosource/videosource.h>
#include <videosource/iniparser.h>

using namespace std;

int main(int argc, char *argv[]) {
    // create a .ini file parser
    IniParser parser(0);

    // Create the video source:
    // 1) construct .ini parser
    // 2) parse .ini file
    // 3) try to construct a valid video source
    VideoSourceFactory factory;
    factory.registerParameters(&parser);

    const char *vs_ini = "videosource.ini";
    const char *name = "added_by_videotrain";
    const char *visual_db_fn = "visual.db";

    for (int i=1; i<argc-1; ++i) {
        if (strcmp(argv[i],"-vs")==0) {
            vs_ini = argv[++i];
        } else if (strcmp(argv[i], "-name") == 0) {
            name = argv[++i];
        } else if (strcmp(argv[i], "-v") == 0) {
            visual_db_fn = argv[++i];
        }
    }

    if (!parser.parse(vs_ini)) {
        cerr << "warning: unable to read " << vs_ini << ", generating an example one.\n";
        parser.dumpExampleFile(const_cast<char *>(vs_ini));
    }

    // try to get a video source
    VideoSource *vs = factory.construct();

    if (vs==0) {
        cerr << "Unable to open video source!\n";
        return 2;
    }

    int width;
    int height;
    vs->getSize(width, height);
    int work_width = width / 2;
    int work_height = height / 2;

    kpt_tracker tracker(work_width, work_height, 5, 10);

    // Loads quantization tree and track clusters
    visual_database database(id_cluster_collection::QUERY_IDF_NORMALIZED);
    if (!database.open(visual_db_fn)) {
        cerr << "Can't open: " << visual_db_fn;
        return 2;
    }

    tracker.load_from_db(database.get_sqlite3_db());

    IplImage *color_image = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 3);
    IplImage *gray_image = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 1);
    IplImage *smaller_image = cvCreateImage(cvSize(work_width, work_height), IPL_DEPTH_8U, 1);
    int num_frame = -1;
    int last_frame = -1;
    while (1) {
        if (!vs->getFrame(color_image)) {
            break;
        }
        if (vs->getId() <= last_frame) {
            break;
        }
        last_frame = vs->getId();

        cvCvtColor(color_image, gray_image, CV_RGB2GRAY);
        IplImage *im = cvCreateImage(cvSize(work_width, work_height), IPL_DEPTH_8U, 1);
        cvPyrDown(gray_image, im);

        ++num_frame;

        // process_frame will detect points and take care of calling cvReleaseImage(im) when required.
        tracker.process_frame_pipeline(im);
        cout << "Frame " << num_frame << endl;
    }

    cout << "Adding object " << name << endl;
    visual_object *obj = database.create_object(name, 0);
    for (int i = 0; i < num_frame; ++i) {
        pyr_frame *frame = (pyr_frame *) tracker.get_nth_frame(i);

        if (i == 0) {
            obj->add_frame(frame);
        } else {
            for (tracks::keypoint_frame_iterator it=frame->points.begin(); !it.end(); ++it) 
            {
                pyr_keypoint * p = (pyr_keypoint *)  it.elem();
                obj->add_keypoint(p, obj->representative_image);
            }
        }
    }
    obj->prepare();
    database.add_to_index(obj);
    return 0;
}
