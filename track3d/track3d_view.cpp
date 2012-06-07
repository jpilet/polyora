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
#include <fstream>

#include "track3d_view.h"
#include <QDir>
#include <QInputDialog>
#include <map>
#include <polyora/timer.h>
#include <math.h>
#include <QTextEdit>

#include "camera.h"

#ifndef M_2PI
#define M_2PI 6.283185307179586476925286766559f
#endif
#ifndef M_PI
#define M_PI 3.141592653589793238462643383279f
#endif

using namespace std;

const int fps_stat_bin_size=20;

void gl_hash_mark(unsigned code, float x, float y, float orient=0, float r = 5);


using namespace std;
track3d_view::track3d_view(QWidget *parent, const char *name, VideoSource *vs)
	: GLBox(parent, name), vs(vs),  database(id_cluster_collection::QUERY_IDF_NORMALIZED)
{
	timer=startTimer(0);
	filter = false;
	record=false;
	record_movie=false;
	viewScores=false;
	learning = true;

	tree_fn = 0;
	clusters_fn = "clusters.bin";
	descriptors_fn = "descriptors.dat";
	visual_db_fn = "visual.db";

	//query_flags = incremental_query::QUERY_IDF|incremental_query::QUERY_NORMALIZED_FREQ;

	nb_missing_frames=0;
	auto_index = false;

	nbLev = 10;
	tracker=0;
	frameCnt=0;
	frameno=0;
	threshold = .06;
	im=im2=0;

	draw_flags = DRAW_COLOR | DRAW_INSTANCE;

	int w,h;
	if (vs) {
		int w,h;
		vs->getSize(w,h);

		resize(w,h);

		while ((h/(1<<nbLev)) < 16) {
			nbLev--;
		}
		std::cout << "Video source: " << w << "x"<<h<<", working with " << nbLev << " levels.\n"; 

		im = cvCreateImage(cvSize(w,h), IPL_DEPTH_8U, 3);
		im2 = cvCreateImage(cvSize(w,h), IPL_DEPTH_8U, 3);
		vs->getFrame(im);
		vs->start();
		setImage(im);
	}
}

track3d_view::~track3d_view() {

	if (im) cvReleaseImage(&im);
	if (im2) cvReleaseImage(&im2);
	if (vs) delete vs;
	if (tracker) delete tracker;
}

void track3d_view::timerEvent(QTimerEvent *) {


	if (im==0) return;
	createTracker();

	TaskTimer::pushTask("main loop");

	makeCurrent();

	float fps=0;
	int ms = qtime.elapsed();
	if (frameCnt>0 && ms > 500) {
		ms = qtime.restart();
		fps = 1000.0f*frameCnt / ms;
		frameCnt=0;
		char str[512];
		sprintf(str,"Visual Object Tracker, Julien Pilet, %3.1f fps", fps);
		setWindowTitle(str);
	}

	static int n=0;
	if (vs) {
		int w,h;
		vs->getSize(w,h);
		int lastId = vs->getId();


		TaskTimer::pushTask("Fetch frame");
		swap(im,im2);
		vs->getFrame(im);

		if (lastId > vs->getId()) {
			// video looped.
			summary();
			close();
			return;
		}

		frameCnt++;
		IplImage *frame = cvCreateImage(cvSize(w,h), IPL_DEPTH_8U, 1);
		cvCvtColor(im,frame,CV_BGR2GRAY);
		//cvSmooth(frame,frame);
		TaskTimer::popTask();

		makeCurrent();

		QTime frame_processing;
		frame_processing.restart();
		float frame_processing_time=0;

		TaskTimer::pushTask("Process frame");
		vobj_frame *pframe = static_cast<vobj_frame *>(tracker->process_frame_pipeline(frame));

		if (pframe)
			setImage(pframe->pyr->images[0]);


		tracker->remove_unmatched_tracks(tracker->get_nth_frame(2));
		tracks::frame_iterator it = tracker->get_nth_frame_it(512);
		tracker->remove_frame(it);

		frame_processing_time = frame_processing.elapsed();
		if (pframe) update_fps_stat(frame_processing_time, pframe);

		if (record) {
			char fn[256];
			sprintf(fn,"out/frame%04d.bmp", n++);
			renderAndSave(fn, im->width, im->height);
			//cvSaveImage(fn, image);
		}

		TaskTimer::popTask();

		update();

		if (record_movie) {
			char fn[256];
			sprintf(fn,"out/frame%04d.bmp", frameno++);
			renderAndSave(fn,0,0);
		}

	}
	TaskTimer::popTask();
}

void track3d_view::update_fps_stat(float fr_ms, pyr_frame *pframe)
{
	unsigned hbin = pframe->points.size() / fps_stat_bin_size;
	fps_stat_map::iterator it(fps_stat.find(hbin));
	if (it != fps_stat.end()) {
		it->second.first = (it->second.second*it->second.first + fr_ms) / (it->second.second+1);
		it->second.second++;
	} else {
		fps_stat[hbin] = pair<double,unsigned>(fr_ms,1);
	}
}


void track3d_view::saveCurrentFrame(const char *filename)
{
	cvSaveImage(filename, image);
}

static inline bool toggle(unsigned &flags, unsigned bit) { return flags = flags ^ bit; }


void track3d_view::keyPressEvent(QKeyEvent *k)
{
	static int n=0;
	char fn[256];
	vobj_frame *frame = (vobj_frame *) tracker->get_nth_frame(0);

	switch (k->key()) {
		case Qt::Key_1: draw_flags = DRAW_COLOR|DRAW_TRACKS; break;
		case Qt::Key_2: draw_flags = DRAW_COLOR|DRAW_KEYPOINTS|DRAW_DARK; break;
		case Qt::Key_3: draw_flags = DRAW_COLOR|DRAW_MATCHES|DRAW_DARK; break;
		case Qt::Key_4: draw_flags = DRAW_COLOR|DRAW_INSTANCE; break;
		case Qt::Key_C: toggle(draw_flags, DRAW_COLOR); break;
		case Qt::Key_D: toggle(draw_flags, DRAW_DARK); break;
		//case Qt::Key_S: sprintf(fn,"frame%04d.png", n++); saveCurrentFrame(fn); break;
		case Qt::Key_S: sprintf(fn,"shot%04d.png", n++); renderAndSave(fn, im->width*2, im->height); break;
		case Qt::Key_Tab: if (vs->isPlaying()) vs->stop(); else vs->start(); break;
		case Qt::Key_R: record = !record;  break;
		case Qt::Key_I: tracker->use_incremental_learning = !tracker->use_incremental_learning; break;
		case Qt::Key_Backspace: tracker->remove_visible_objects_from_db(frame); break;
				/*
		case Qt::Key_B: query->switch_flag(incremental_query::QUERY_BIN_FREQ);
				entry = 0; 
				init_query_with_frame(*query, frame); 
				cout << "Flags are now " << query->get_flags() << endl;
				break;
				*/
		case Qt::Key_G:  toggle(draw_flags, DRAW_INSTANCE); break;
		case Qt::Key_F:  if (isFullScreen()) showNormal(); else showFullScreen(); break;

		case Qt::Key_M:  if(k->modifiers() & Qt::ShiftModifier) {
					 record_movie = !record_movie;
				 } else {
					 toggle(draw_flags, DRAW_MATCHES);
				 }
				 /*
				query->switch_flag(incremental_query::QUERY_MIN_FREQ);
				entry = 0; 
				init_query_with_frame(*query, frame); 
				cout << "Flags are now " << query->get_flags() << endl;
				*/
				break;
				/*
		case Qt::Key_N: query->switch_flag(incremental_query::QUERY_NORMALIZED_FREQ);
				entry = 0; 
				init_query_with_frame(*query, frame); 
				cout << "Flags are now " << query->get_flags() << endl;
				break;
				*/
		case Qt::Key_Space: if (timer) {killTimer(timer); timer=0; } 
					    else { timer = startTimer(0); } break;
		case Qt::Key_Q: summary(); close(); break;
		case Qt::Key_Escape: selected_kpt=0; break;
		//case Qt::Key_A: print_affinity(); break;
		//case Qt::Key_F: add_current_frame_to_db("Interactive, F-Mat checking", visual_object::VERIFY_FMAT); break;
		case Qt::Key_H: add_current_frame_to_db("Interactive, homography checking", visual_object::VERIFY_HOMOGRAPHY); break;
		case Qt::Key_Plus: threshold += .005; cout << "T=" << threshold << endl; break;
		case Qt::Key_Minus: threshold -= .005; cout << "T=" << threshold << endl; break;
		case Qt::Key_Z: zoom(1,1); translate(0,0); resize(frame->pyr->images[0]->width, frame->pyr->images[0]->height); break;
	}
	update();
}



void track3d_view::add_current_frame_to_db(const char *name, int flags)
{
	pyr_frame *frame = (pyr_frame *)tracker->get_nth_frame(0);

	cout << "Adding object " << name << endl;
	visual_object *obj = database.create_object(name, flags);
	obj->add_frame(frame);
	obj->prepare();
	database.add_to_index(obj);
	add_frame_time.restart();
}


void track3d_view::createTracker()
{
	if (tracker==0) {
		if (im==0) return;

		// make sure the OpenGL context is ready, to let kpt_tracker use the GPU.
		makeCurrent();
		cout << "Loading object db..." << flush;
		database.open(visual_db_fn);
		cout << "done.\n";
		tracker = new vobj_tracker(im->width,im->height,nbLev, (int)(16), &database, true);
		tracker->use_incremental_learning = learning;

		if (tree_fn) {
			// old style plain file loading
			if (tracker->load_tree(tree_fn)) {
				cout << tree_fn << ": tree has " << tracker->tree->count_leafs() << " leafs.\n";
				if (tracker->load_clusters(clusters_fn))
					cout << clusters_fn << ": clusters loaded.\n";
			}
		} else {
			// new sqlite3 style
			cout << "Loading tree from db..." << flush;
			if (tracker->load_tree(database.get_sqlite3_db())) {
				cout << "done, loaded " << tracker->tree->count_leafs() << " leafs.\n";
			} else {
				cout << "failed\n";
			}
			cout << "Loading clusters from db..." << flush;
			if (tracker->load_clusters(database.get_sqlite3_db())) {
				cout << "done.\n";
			} else {
				cout << "failed.\n";
			}
		}
		cout << "Ready.\n";

		// These constants come from opencv's calibration file.
		camera.set(640, 480, // image_width, image_height
			// the following are the fields of camera_matrix:
			// data[0], data[4], data[2], data[5]
			6.3802737002212859e+02, 6.2878307248533815e+02,
			3.2879300246053975e+02, 2.2232560297939980e+02);
		if (!camera.loadOpenCVCalib("out_camera_data.yml")) {
			cout << "Failed to load out_camera_data.yml. Using default values.\n";
		}
		camera.flip();
	}
}

static void setupHomography(const float H[3][3])
{
	float m[4][4];

	//glLoadIdentity();
	m[0][0]=H[0][0];  m[1][0]=H[0][1];  m[2][0]=0;  m[3][0]=H[0][2];
	m[0][1]=H[1][0];  m[1][1]=H[1][1];  m[2][1]=0;  m[3][1]=H[1][2];
	m[0][2]=0;        m[1][2]=0;        m[2][2]=1;  m[3][2]=0;
	m[0][3]=H[2][0];  m[1][3]=H[2][1];  m[2][3]=0;  m[3][3]=H[2][2];

	glMultMatrixf(&m[0][0]);
	// homographic transform is now correctly loaded. Following
}

GLuint track3d_view::get_texture_for_obj(visual_object *obj)
{
	GLuint texture=0;
	static QStringList textures;
	
	if (textures.size()==0) {
		QDir texdir("textures");
		QStringList filters;
		filters << "*.dds";
		texdir.setNameFilters(filters);
		textures = texdir.entryList();
		cout << "Found " << textures.size() << " dds files.\n";
	}

	if (textures.size()>0) {
		unsigned long id = obj->get_id();
		unsigned long texno = id % textures.size();
		//cout << "trying texture: " << textures[texno].toLatin1().constData() << endl;
		texture = bindTexture("textures/"+textures[texno]);
	}
	return texture;
}

void track3d_view::augment3d(visual_object *obj, float H[3][3]) {

	// If the video mode has changed, the camera should be re-calibrated.
	// Otherwise, it is possible to set a new resolution:
	// camera.resizeImage( width, height );

	// Update the pose
	double Hd[3][3];
	for (int i = 0; i< 3; ++i) {
	    for (int j = 0; j< 3; ++j) {
		Hd[i][j] = H[i][j];
	    }
	}

	PerspectiveCamera cam(camera);
	cam.setPoseFromHomography(Hd);
	cam.flip();

	// cout << camera << endl;

	double matrix[4][4];
	glMatrixMode(GL_PROJECTION);
	cam.getGlProjection(matrix);
	glPushMatrix();
	glLoadMatrixd(&matrix[0][0]);

	glMatrixMode(GL_MODELVIEW);
	cam.getGlModelView(matrix);
	glPushMatrix();
	glLoadMatrixd(&matrix[0][0]);

	glBegin(GL_LINES);
	glVertex3f(0,0,0);
	glVertex3f(640,0,0);
	glVertex3f(0,0,0);
	glVertex3f(0,480,0);
	glVertex3f(0,0,0);
	glVertex3f(0,0,500);
	glEnd();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

}


void track3d_view::draw_instances(vobj_frame *frame)
{
	for (vobj_instance_vector::iterator it(frame->visible_objects.begin());
			it != frame->visible_objects.end(); ++it)
	{
		if (it->object->get_flags() & visual_object::VERIFY_HOMOGRAPHY) {
			augment3d(it->object, it->transform);
		}
	}
}

void track3d_view::paintGL() 
{
	TaskTimer::pushTask("Display");

#ifdef WITH_SIFTGPU
	glDisable(GL_TEXTURE_RECTANGLE_ARB);
#endif
	icon_texture_available.splice(icon_texture_available.begin(), icon_texture_used);

	glDisable(GL_DEPTH_TEST);
	resizeGL(width(),height());

	if (draw_flags & DRAW_DARK) glColor4f(.5,.5,.5,1);
	else glColor4f(1,1,1,1);

	if (draw_flags & DRAW_COLOR)
		setImage(im2);

	GLBox::paintGL();

	if (tracker==0) {
		TaskTimer::popTask();
		return;
	}

	int t = add_frame_time.elapsed();
	if (t>0 && t< 1200) {
		GLuint tex = bindTexture("add_frame.dds");
		if (tex) {
			glColor4f(1,1,1,1);
			glEnable(GL_BLEND);
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, tex);
			glBegin(GL_QUADS);
			glTexCoord2f(0,0);
			glVertex2f(10,10);
			glTexCoord2f(1,0);
			glVertex2f(256,10);
			glTexCoord2f(1,1);
			glVertex2f(256,64);
			glTexCoord2f(0,1);
			glVertex2f(10,64);
			glEnd();
		}
	}

	vobj_frame *frame = (vobj_frame *) tracker->get_nth_frame(0);
	if (!frame) {
		TaskTimer::popTask();
		return;
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	setAxisUp(false);
	setImageSpace();

	//draw_keypoints(frame);

	//if (draw_flags & DRAW_INSTANCE)
	draw_instances(frame);

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
#ifdef WITH_SIFTGPU
	glEnable(GL_TEXTURE_RECTANGLE_ARB);
#endif
	TaskTimer::popTask();
}

void track3d_view::show_track(pyr_keypoint *k)
{
	// count the number of different ids for current track
	std::map<unsigned, unsigned> map, cmap;
	int track_len= 0;
	if (k->track) track_len = k->track->length;
	cout << "Track has " << track_len << " frames:\n";
	for (tracks::keypoint_match_iterator it(k); !it.end(); --it) {
		pyr_keypoint *p = (pyr_keypoint *)it.elem();
		cout << "  " << p->id << ":" << p->cid << "("<<p->cscore<<")";

		std::map<unsigned, unsigned>::iterator id_it = map.find(p->id);
		if (id_it == map.end()) map[p->id] = 1;
		else id_it->second++;

		id_it = cmap.find(p->cid);
		if (id_it == cmap.end()) cmap[p->cid] = 1;
		else id_it->second++;

	}
	cout << endl << "ID histogram:\n";

	for (std::map<unsigned,unsigned>::iterator id_it = map.begin();
			id_it != map.end(); ++id_it)
	{
		cout << "  id:" << id_it->first << " on " << id_it->second << " frames.\n";
	}
	if (k->track) {
		/*
		cout << "should be similar to:\n";
		pyr_track *track = (pyr_track *) k->track;
		for(id_cluster::uumap::iterator it=track->id_histo.histo.begin();
				it!=track->id_histo.histo.end(); ++it)
		{
			cout << "  id:" << it->first << " on " << it->second << " frames.\n";
		}
		*/
		float d;
		pyr_track *track = (pyr_track *) k->track;
                if (tracker->id_clusters) {
                    id_cluster *c = tracker->id_clusters->get_best_cluster(&track->id_histo.query_cluster, &d);
                    cout << "Best cluster: " << c->id << " score= " << d << endl;
                }
	} else {
		cout << "pointer to track is 0\n";
	}

	for (std::map<unsigned,unsigned>::iterator id_it = cmap.begin();
			id_it != cmap.end(); ++id_it)
	{
		cout << "  cid:" << id_it->first << " on " << id_it->second << " frames.\n";
	}


}

void track3d_view::show_tracks()
{
	int t=1;
	pyr_frame *frame = (pyr_frame *) tracker->get_nth_frame(0);

	std::map<unsigned, unsigned> full_map;

	for (tracks::keypoint_frame_iterator it(frame->points.begin()); !it.end(); ++it) {
		pyr_keypoint *k = (pyr_keypoint *) it.elem();

		if (k->track_is_longer(10)) {

			// count the number of different ids for current track
			std::map<unsigned, unsigned> map;
			int track_len=0;
			for (tracks::keypoint_match_iterator it(k); !it.end(); --it) {
				pyr_keypoint *p = (pyr_keypoint *)it.elem();
				std::map<unsigned, unsigned>::iterator id_it = map.find(p->id);
				if (id_it == map.end())
					map[p->id] = 1;
				else
					id_it->second++;

				track_len++;
			}
			cout << "Track " << t++ << " " << track_len << " frames:\n";
			for (std::map<unsigned,unsigned>::iterator id_it = map.begin();
					id_it != map.end(); ++id_it)
			{
				cout << "  id:" << id_it->first << " on " << id_it->second << " frames.\n";
				std::map<unsigned, unsigned>::iterator f_it = full_map.find(id_it->first);
				if (f_it == full_map.end())
					full_map[id_it->first] = id_it->second;
				else
					f_it->second += id_it->second;
				
			}
		}
	}

	cout << "total, " << full_map.size() << " different ids detected:\n";
	for (std::map<unsigned,unsigned>::iterator id_it = full_map.begin();
			id_it != full_map.end(); ++id_it)
	{
				cout << "  id:" << id_it->first << " on " << id_it->second << " frames.\n";
	}
}

void track3d_view::summary()
{

	TaskTimer::printStats();

	cout <<"FPS vs number of features:\n";
	for (fps_stat_map::iterator it(fps_stat.begin()); it!=fps_stat.end(); ++it)
	{
		cout << it->first*fps_stat_bin_size << ".." << ((1+it->first)*fps_stat_bin_size)-1 << ": " 
			<< it->second.first << " ms (count=" << it->second.second << ")\n";
	}
	cout << endl;


}


