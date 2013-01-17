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
#include "vsview.h"
#include <QInputDialog>
#include <map>
#include <polyora/timer.h>
#include <math.h>

#ifdef WIN32
#define finite _finite
#endif

#ifndef M_2PI
#define M_2PI 6.283185307179586476925286766559f
#endif
#ifndef M_PI
#define M_PI 3.141592653589793238462643383279f
#endif

using namespace std;

namespace {

const int fps_stat_bin_size=20;

void gl_hash_mark(unsigned code, float x, float y, float orient=0, float r = 5);

struct id_pair {
    unsigned a, b;
    id_pair(unsigned id1, unsigned id2) {
        if (id1 < id2) {
            a= id1;
            b= id2;
        } else {
            a=id2;
            b=id1;
        }
    }

    bool operator< (const id_pair &p)const 
    {
        if (a < p.a) return true;
        if (a ==  p.a) return b < p.b;
        return false;
    }
};

void descriptorToImage(float *descriptor, CvMat* image) {
    cv::Mat im;
    patch_tagger::singleton()->unproject(descriptor, &im);
    *image = im;
}

}  // namespace

map<id_pair,unsigned> affinity_matrix;


VSView::VSView(QWidget *parent, const char *name, VideoSource *vs)
	: GLBox(parent, name), vs(vs),  database(id_cluster_collection::QUERY_IDF_NORMALIZED),
	query(0)
{
	descrf=0;
	timer=startTimer(0);
	filter = false;
	record=false;
	record_pts=false;
	record_movie=false;
	viewScores=false;

	tree_fn = 0;
	clusters_fn = "clusters.bin";
	descriptors_fn = "descriptors.dat";
	visual_db_fn = "visual.db";

	help_window = 0;

	//query_flags = incremental_query::QUERY_IDF|incremental_query::QUERY_NORMALIZED_FREQ;

	entry=0;
	nb_missing_frames=0;
	auto_index = false;
	dark = false;

	nbLev = 10;
	tracker=0;
	frameCnt=0;
	frameno=0;
	threshold = .06;

	selected_kpt = 0;
	gt_success = gt_fail = 0;
	false_pos = false_neg = 0;
	true_neg = 0;

	nlost=0;

	use_pipeline = false;

	//view_mode = VIEW_AUTO;
	view_mode = VIEW_ANNOTATIONS;

	im = 0;

	int w,h;
	if (vs) {
		vs->getSize(w,h);

		zoom(.5,1);
		resize(w/.5,h);

		while ((h/(1<<nbLev)) < 16) {
			nbLev--;
		}
		std::cout << "Video source: " << w << "x"<<h<<", working with " << nbLev << " levels.\n"; 

		im = cvCreateImage(cvSize(w,h), IPL_DEPTH_8U, 3);
		vs->getFrame(im);
		vs->start();
		setImage(im);

	}
}

VSView::~VSView() {
	if (query)
		delete query;

	if (im) cvReleaseImage(&im);
	if (vs) delete vs;
	if (descrf) fclose(descrf);
	if (help_window) delete help_window;
}

void VSView::timerEvent(QTimerEvent *) {

	TaskTimer::pushTask("main loop");

	if (im==0) return;
	createTracker();

	makeCurrent();

	float fps=0;
	int ms = qtime.elapsed();
	if (frameCnt>0 && ms > 500) {
		ms = qtime.restart();
		fps = 1000.0f*frameCnt / ms;
		frameCnt=0;
		char str[512];
		sprintf(str,"QtPolyora, Julien Pilet, %3.1f fps", fps);
		setWindowTitle(str);
	}

	static int n=0;
	if (vs) {
		int w,h;
		vs->getSize(w,h);
		int lastId = vs->getId();
		if (lastId == 1) total_time.start();


		TaskTimer::pushTask("Fetch frame");
		vs->getFrame(im);

		if (lastId > vs->getId()) {
			// video looped.
			summary();
			close();
			return;
		}

		frameCnt++;
		setImage(im);
		IplImage *frame = cvCreateImage(cvSize(w,h), IPL_DEPTH_8U, 1);
		cvCvtColor(im,frame,CV_BGR2GRAY);
		//cvSmooth(frame,frame);
		TaskTimer::popTask();

		makeCurrent();
		pyr_frame *pframe = 0;

		QTime frame_processing;
		frame_processing.restart();
		float frame_processing_time=0;

		TaskTimer::pushTask("Process frame");

		if (use_pipeline) {
			pframe = tracker->process_frame_pipeline(frame, vs->getId());
			if (pframe)
				setImage(pframe->pyr->images[0]);
		}else
			pframe = tracker->process_frame(frame, vs->getId());
			//frame_processing_time = frame_processing.elapsed();

		if (record_pts) {
			save_descriptors(pframe);
		} else {

			//segment_scene();

			if (pframe) {
			TaskTimer::pushTask("Query");
			TaskTimer::pushTask("Query update");
			if (!query) {
				query = database.create_incremental_query();
				//query->set_all_flags(query_flags);
				//query->set_flag(incremental_query::QUERY_NORMALIZED_FREQ | incremental_query::QUERY_BIN_FREQ);
				//query->set_flag(incremental_query::QUERY_NORMALIZED_FREQ | incremental_query::QUERY_MIN_FREQ);
				init_query_with_frame(*query, pframe);
			} else {
				update_query_with_frame(*query, tracker);
			}
			TaskTimer::popTask();
			TaskTimer::pushTask("Sorting results and fetching correspondences");
			float score;
			entry = (visual_object *)query->get_best(&score);
			if (entry) {
				float r =entry->get_correspondences(pframe, corresp);
				cout << "Frame #" << vs->getId() << ": score=" << score;
				cout << " r= " << r << " retrieved: '" << entry->get_comment() << "', gt: '" 
					<< ground_truth << "'" << endl;
				if (r<threshold) entry=0;
			} 
			TaskTimer::popTask();
			TaskTimer::popTask();

			if (entry) nb_missing_frames=0;
			else nb_missing_frames++;

			if (use_pipeline)
				script_exec(vs->getId()-1);
			else
				script_exec(vs->getId());

			if (auto_index && entry==0 && nb_missing_frames > 10) 
				add_current_frame_to_db("auto", 0);

			if (record) save_tracks();
			}
			//cmp_affinity();
		}

		tracker->remove_unmatched_tracks(tracker->get_nth_frame(2));
		if (!record) {
			tracks::frame_iterator it = tracker->get_nth_frame_it(512);
			tracker->remove_frame(it);
		}

		frame_processing_time = frame_processing.elapsed();
		update_fps_stat(frame_processing_time, pframe);

		if (selected_kpt) {
			selected_kpt = (pyr_keypoint *) selected_kpt->matches.next;
			if (selected_kpt) cout << "id: "<<selected_kpt->id << " cid: " << selected_kpt->cid << endl;
		}

		if (0) {
			char fn[256];
			sprintf(fn,"out/frame%04d.bmp", n++);
			cvSaveImage(fn, image);
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

void VSView::update_fps_stat(float fr_ms, pyr_frame *pframe)
{
	if (!pframe) return;

	unsigned hbin = pframe->points.size() / fps_stat_bin_size;
	fps_stat_map::iterator it(fps_stat.find(hbin));
	if (it != fps_stat.end()) {
		it->second.first = (it->second.second*it->second.first + fr_ms) / (it->second.second+1);
		it->second.second++;
	} else {
		fps_stat[hbin] = pair<double,unsigned>(fr_ms,1);
	}
}



void VSView::saveCurrentFrame(const char *filename)
{
	cvSaveImage(filename, image);
}
static const char *help =
"Key mapping:\n"
"--- Database editing ---\n"
"a: add current view without geometric check\n"
"h: add current view with Homography check\n"
"f: add current view with F-Mat check\n"
"backspace - delete visible object(s) from DB\n"
"+/-: increase/decrease detection threshold\n"
"\n--- Video/Processing control ---\n"
"tab - pause/resume video input\n"
"space - pause/resume processing\n"
"\n--- Display ---\n"
"1: Normal display\n"
"2: Keypoint display\n"
"3: Track display\n"
"4: Show annotations\n"
"d: toggles image darkening for better constrast\n"
"l/L: increase/decrease displayed pyramid level\n"
"\n--- Keypoint selection ---\n"
"click - select a point\n"
"esc - unselect point\n"
"\n--- Miscellaneous ---\n"
"s: save a screen shot as 'shot0000.png'\n"
"r: toggle track recording\n"
"p: toggle descriptor recording\n"
"q: quit the program\n"
"z: reset view\n";

void VSView::show_help()
{
	if (help_window==0) {
		QMessageBox *box = new QMessageBox(QMessageBox::NoIcon,
				"Visual Object Tracker Help", help, 
				QMessageBox::Ok, this,
				Qt::Dialog);
		box->resize(100,600);
		help_window = box;
	}
	help_window->show();
	help_window->raise();
	help_window->activateWindow();
}


void VSView::keyPressEvent(QKeyEvent *k)
{
	static int n=0;
	char fn[256];
	pyr_frame *frame = (pyr_frame *) tracker->get_nth_frame(0);

	switch (k->key()) {
		case Qt::Key_D: dark = !dark; break;
		//case Qt::Key_S: sprintf(fn,"frame%04d.png", n++); saveCurrentFrame(fn); break;
		case Qt::Key_S: sprintf(fn,"shot%04d.png", n++); renderAndSave(fn, im->width*2, im->height); break;
		case Qt::Key_Tab: if (vs->isPlaying()) vs->stop(); else vs->start(); break;
		case Qt::Key_R: record = !record; record_pts=false; break;
		case Qt::Key_L: if (k->modifiers() && Qt::ShiftModifier) {
					if (viewlevel<nbLev-1) viewlevel++;
				} else {
					if (viewlevel>0) viewlevel--;
				}
				break;
		//case Qt::Key_T: show_tracks(); break;
				/*
		case Qt::Key_B: query->switch_flag(incremental_query::QUERY_BIN_FREQ);
				entry = 0; 
				init_query_with_frame(*query, frame); 
				cout << "Flags are now " << query->get_flags() << endl;
				break;
				*/
		case Qt::Key_M:  if(k->modifiers() & Qt::ShiftModifier) {
					 record_movie = !record_movie;
					 break;
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
		case Qt::Key_A: add_current_frame_to_db("Interactive, no geometry checking", 0);  break;
		case Qt::Key_F: add_current_frame_to_db("Interactive, F-Mat checking", visual_object::VERIFY_FMAT); break;
		case Qt::Key_H: add_current_frame_to_db("Interactive, homography checking", visual_object::VERIFY_HOMOGRAPHY); break;
		case Qt::Key_P: record_pts = !record_pts; record=false; break;
		case Qt::Key_C: entry = 0; init_query_with_frame(*query, frame); break;
		case Qt::Key_1: view_mode = VIEW_AUTO; break;
		case Qt::Key_2: view_mode = VIEW_KEYPOINTS; break;
		case Qt::Key_3: view_mode = VIEW_TRACKS; break;
		case Qt::Key_4: view_mode = VIEW_ANNOTATIONS; break;
		case Qt::Key_I: auto_index = !auto_index; break;
		case Qt::Key_Plus: threshold += .005; cout << "T=" << threshold << endl; break;
		case Qt::Key_Minus: threshold -= .005; cout << "T=" << threshold << endl; break;
		case Qt::Key_Z: zoom(.5,1); translate(0,0); /*resize(frame->pyr->images[0]->width, frame->pyr->images[0]->height); */break;
		default: if ((k->key()>= Qt::Key_F1 && k->key()<=Qt::Key_F12) || k->text().size() > 0)
				 show_help();
	}
	update();
}

void VSView::segment_scene()
{

	pyr_frame *lf= (pyr_frame *)tracker->get_nth_frame(1);
	if (!lf) return;
	
	//for (ttrack *t= tracker->all_tracks; t!=0; t=t->track_node.next) {
	//}

	for (tracks::keypoint_frame_iterator it(lf->points.begin()); !it.end(); ++it) {
		pyr_keypoint *k = (pyr_keypoint *) it.elem();
		if (k->track && k->track->length > 20) {
			if (k->matches.next==0) nlost++;
		}
	}
	//cout << "n=" << n << " lost=" << n_lost << endl;
	if (nlost > 30) {
		nlost = 0;
		cout << "scene change " << endl;
		for (ttrack *t= tracker->all_tracks; t!=0; t=t->track_node.next) {
			if (t->length > 10) {
				pyr_keypoint *p= (pyr_keypoint *)t->keypoints;
				cout << p->cid << ",";
			}
		}
		tracks::frame_iterator it = tracker->get_nth_frame_it(1);
		tracker->remove_frame(it);
	}

}

void VSView::add_current_frame_to_db(const char *name, int flags)
{
	pyr_frame *frame = (pyr_frame *)tracker->get_nth_frame(0);
	if (auto_index) {
		// check that current frame is good enough for indexing
		int n=0;
		for (tracks::keypoint_frame_iterator it(frame->points.begin()); !it.end(); ++it) {
			pyr_keypoint *k = (pyr_keypoint *) it.elem();
			if (k->track_is_longer(10)) n++;
		}
		if (n < 30) return;
		cout << "Auto-indexing a frame with " << n << " tracks longer than 4 frames.\n";
	}

	cout << "Adding object " << name << endl;
	visual_object *obj = database.create_object(name, flags);
	obj->add_frame(frame);
	obj->prepare();
	database.add_to_index(obj);
	init_query_with_frame(*query, frame);
	add_frame_time.restart();
}

bool VSView::selectKeypoint(QMouseEvent *event)
{
	point2d c;
	screenToImageCoord(event->x(), event->y(), &c.u, &c.v);

	pyr_frame *frame = (pyr_frame *) tracker->get_nth_frame(0);

	int w = frame->pyr->images[0]->width;
	if (c.u > w) {
		addAnnotation(c.u-w,c.v);
		return false;
	}

	bucket2d<tkeypoint>::iterator it = frame->points.search(c.u,c.v,5);
	if (it.end()) return false;
	
	float dist = c.dist(*it.elem());
	bucket2d<tkeypoint>::iterator best = it;
	for(++it; !it.end(); ++it) {
		float d= c.dist(*it.elem());
		if (d<dist) {
			dist = d;
			best = it;
		}
	}
	selected_kpt = (pyr_keypoint *) best.elem();
	show_track(selected_kpt);

	return true;
}

void VSView::mouseDoubleClickEvent ( QMouseEvent * ) {
	selected_kpt=0;
	update();
}

void VSView::mousePressEvent ( QMouseEvent * event )
{
	switch (event->button()) {
		case Qt::LeftButton:
			if (!selected_kpt) {
				selectKeypoint(event);
			}
			break;
		default:
			break;
	}
	GLBox::mousePressEvent(event);
	update();
}

void VSView::mouseReleaseEvent ( QMouseEvent * event )
{
	GLBox::mouseReleaseEvent(event);
}

static inline void glVertex(cv::Point_<float> p)
{
	glVertex2f(p.x,p.y);
}

void draw_keypoint(const pyr_keypoint *k) {
	float x = k->u;
	float y = k->v;
	float angle = k->descriptor.orientation;
	float len=5 +   (2 << k->scale);
	float dx = len * cosf(angle);
	float dy = len * sinf(angle);

	if (k->cid) {
		gl_hash_mark(k->cid, k->u, k->v, k->descriptor.orientation, len);
	} else {
#ifdef WITH_MSER
		glBegin(GL_LINE_STRIP);
		glVertex(k->mser.c -(k->mser.e1) -(k->mser.e2));
		glVertex(k->mser.c -k->mser.e1 +k->mser.e2);
		glVertex(k->mser.c +k->mser.e1 +k->mser.e2);
		glVertex(k->mser.c +k->mser.e1 -k->mser.e2);
		glVertex(k->mser.c -k->mser.e1 -k->mser.e2);
		glEnd();
#endif
		glBegin(GL_LINES);
		glVertex2f(x,y);
		glVertex2f(x+dx,y+dy);
		glVertex2f(x-dy/4,y+dx/4);
		glVertex2f(x+dy/4,y-dx/4);
		glEnd();
	}
}

void VSView::createTracker()
{
	if (tracker==0) {
		if (im==0) return;

		// make sure the OpenGL context is ready, to let kpt_tracker use the GPU.
		makeCurrent();
		tracker = new kpt_tracker(im->width,im->height,nbLev, (int)(16), true);

		database.open(visual_db_fn);

		if (tree_fn) {
			tracker->load_tree(tree_fn);
			tracker->load_clusters(clusters_fn);
		} else {
			tracker->load_from_db(database.get_sqlite3_db());
		}

		cout << "Press F1 for help.\n";

		add_frame_tex.setImage(cvLoadImage("add_frame.png"));
	}
}

namespace {

void HSVtoRGB( float *r, float *g, float *b, float h, float s, float v )
{
	int i;
	float f, p, q, t;

	if( s == 0 ) {
		// achromatic (grey)
		*r = *g = *b = v;
		return;
	}

	h /= 60;			// sector 0 to 5
	i = floor( h );
	f = h - i;			// factorial part of h
	p = v * ( 1 - s );
	q = v * ( 1 - s * f );
	t = v * ( 1 - s * ( 1 - f ) );

	switch( i ) {
		case 0: *r = v; *g = t; *b = p; break;
		case 1: *r = q; *g = v; *b = p; break;
		case 2: *r = p; *g = v; *b = t; break;
		case 3: *r = p; *g = q; *b = v; break;
		case 4: *r = t; *g = p; *b = v; break;
		default:		// case 5:
			*r = v; *g = p; *b = q; break;
	}
}

void gl_hash_color(unsigned key)
{
	float c[3];
	HSVtoRGB(c,c+1,c+2, fmod((float)key, 360), 1, 1);
	glColor3fv(c);
}

void gl_hash_mark(unsigned code, float x, float y, float orient, float r)
{
	gl_hash_color(code);

	unsigned char c = (code^(code>>8)^(code>>16)^(code>>24))&0xff;
	int n = 4 + (c&0x7);
	float f = 1.5f + ((c>>3)&0x3)/2.0f;

	float step_angle = M_PI*2.0f/n;
	float angle=orient;

	glBegin(GL_LINE_STRIP);

	for (int i=0; i<n; i++) {

		float r2 = (i&1 ? r*f : r);
		float sin = sinf(angle);
		float cos = cosf(angle);
		angle+=step_angle;

		glVertex2f(x+r2*cos,y+r2*sin);
	}
	float sin = sinf(angle);
	float cos = cosf(angle);
	glVertex2f(x+r*cos,y+r*sin);

	glEnd();
}

}  // namespace

void VSView::draw_selected_track()
{
	if (!selected_kpt) return;

	// search for start of track
	tracks::keypoint_match_iterator it(selected_kpt); 
	while (it.elem()->matches.next) ++it;
	tracks::keypoint_match_iterator first(it); 

	pyr_frame *frame = (pyr_frame *) tracker->get_nth_frame(0);

	if (it.elem()->frame != frame) {
		glColor4f(1,0,0,1); // point lost
		selected_kpt=0;
	} else {
		glColor4f(0,1,0,1); // point tracked
	}

	for (it=first; !it.end(); --it) {
		pyr_keypoint *p = (pyr_keypoint *)it.elem();
		draw_keypoint(p);
	}

	float r = 16;
	point2d pos(0,image->height);
	for (it=first; !it.end(); --it) {
		pyr_keypoint *k = (pyr_keypoint *) it.elem();

		if (k->node) {
            cv::Mat im;
            patch_tagger::unproject(k->node->mean.mean, &im);

			point2d pos_down(pos.u, pos.v+2*r);
			draw_icon(&pos_down, im, r, r, image->width, 0, 2*r);
		}
		{
			float descr[kmean_tree::descriptor_size];
            k->descriptor.array(descr);
            cv::Mat im;
            patch_tagger::unproject(descr, &im);
			point2d pos_down(pos.u, pos.v+r);
			draw_icon(&pos_down, im, r, r, image->width, 0, 2*r);
		}
        CvMat rotated;
        cvInitMatHeader(&rotated, patch_tagger::patch_size, patch_tagger::patch_size, CV_32FC1,
                        k->descriptor._rotated);
		draw_icon(&pos, &rotated, r, r, image->width, 0, 2*r);
	}
	glPopMatrix();
	glPopMatrix();
}

void VSView::draw_icon(point2d *c, cv::Mat image, float w, float h, float max_width, int margin_x, int margin_y)
{
	if (image.empty()) return;

	if (icon_texture_available.begin() == icon_texture_available.end()) {
		IplTexture t;
		icon_texture_available.push_back(t);
	}
	icon_texture_used.splice(icon_texture_used.begin(), icon_texture_available, icon_texture_available.begin());
	IplTexture &t(icon_texture_used.front());
	double min, max;
    cv::minMaxLoc(image, &min, &max);
    cv::Mat converted;
    image.convertTo(converted, CV_8UC1,
		255.0 / (max - min), 
		- (255.0 / (max - min)) * min);

	IplImage* converted_ipl = cvCreateImage(converted.size(), IPL_DEPTH_8U, 1); 
	converted.copyTo(cv::Mat(converted_ipl));
	t.setImage(converted_ipl);
	glColor4f(1,1,1,1);
	t.drawQuad(c->u, c->v, w, h);
	c->u +=  w + margin_x;
	if (c->u>=max_width) {
		c->u = 0;
		c->v += h + margin_y;
	}
}

void VSView::draw_icon(point2d *c, const CvArr *image, float w, float h, float max_width, int margin_x, int margin_y)
{
	if (!image) return;

	if (icon_texture_available.begin() == icon_texture_available.end()) {
		IplTexture t;
		icon_texture_available.push_back(t);
	}
	icon_texture_used.splice(icon_texture_used.begin(), icon_texture_available, icon_texture_available.begin());
	IplTexture &t(icon_texture_used.front());
	CvMat m;
	CvMat* original = cvGetMat(image, &m);
	double min, max;
	cvMinMaxLoc(original, &min, &max);
	IplImage* converted = cvCreateImage(cvGetSize(original), IPL_DEPTH_8U, 1); 
	cvCvtScale(original, converted,
		255.0 / (max - min), 
		- (255.0 / (max - min)) * min);

	t.setImage(converted);
	glColor4f(1,1,1,1);
	t.drawQuad(c->u, c->v, w, h);
	c->u +=  w + margin_x;
	if (c->u>=max_width) {
		c->u = 0;
		c->v += h + margin_y;
	}
	cvReleaseImage(&converted);
}

void VSView::draw_all_tracks(pyr_frame *frame)
{
	glColor4f(0,1,0,1);

	for (tracks::keypoint_frame_iterator it(frame->points.begin()); !it.end(); ++it) {
		pyr_keypoint *k = (pyr_keypoint *) it.elem();

		if (k->track_is_longer(2)) {
			glBegin(GL_LINE_STRIP);

			int n=0;

			// count the number of different ids for current track
			std::map<int, pyr_keypoint *> map;
			int track_len=0;
			for (tracks::keypoint_match_iterator it(k); !it.end(); --it) {
				pyr_keypoint *p = (pyr_keypoint *)it.elem();
				map[p->id] = p;
				track_len++;
			}
			//float c = 1/(map.size());
			float c =(float)map.size()/(float)track_len;
			if (c<.1) c =1;
			else c =0;
			//cout << map.size() << "/" << track_len << ", " << 100.0f*map.size()/(float)track_len << "%.\n";

			for (tracks::keypoint_match_iterator it(k); !it.end(); --it) {
				pyr_keypoint *p = (pyr_keypoint *)it.elem();

				float alpha = 1.0f-(n++)/256.0f;
				//unsigned c = p->descriptor.orientation;
				//glColor4f(c>>2,(c>>1)&1,c&1, alpha);
				if (p->matches.prev) {
					/*
					   pyr_keypoint * prev = ((pyr_keypoint *)(p->matches.prev));

					   printf("%d: %04x is expected to be %04x, dx=%f,dy=%f\n",
					   n,
					   p->descriptor.projection,
					   prev->descriptor.projection,
					   p->u-prev->u, p->v-prev->v
					   );
					   */
					/*
					   unsigned bits = prev->descriptor.projection ^ p->descriptor.projection;
					   unsigned n =0;
					   for (int i=0;i<16;i++) n += (bits>>i)&1;
					   float c = n/4.0f;
					   */
					//float c = (prev->id == p->id ? 1 : 0);

					glColor4f(1-c,c,0,alpha);
				}
				else glColor4f(0,1,0,alpha);
				glVertex2f(p->u, p->v);
			}
			glEnd();
		}
	}
}

void VSView::draw_keypoints(pyr_frame *frame)
{
	for (tracks::keypoint_frame_iterator it(frame->points.begin()); !it.end(); ++it) {
		pyr_keypoint *k = (pyr_keypoint *) it.elem();

		//if (!k->track_is_longer(5)) continue;

		glColor4f(0,1,0,.5);
		draw_keypoint(k);

	}
}

void VSView::draw_entry_image()
{
	if (!entry) return;

	IplImage *entry_image = database.get_image(entry->representative_image);
	entry_tex.setImage(entry_image);

	if (entry_image)
		entry_tex.drawQuad(image->width,0);
}

void VSView::draw_matches(pyr_frame *frame)
{
	if (!entry || entry->get_total() ==0) return;

	IplImage *entry_image = database.get_image(entry->representative_image);

	if (!entry_image)
		entry_image = image;

	point2d cursor(0,image->height);
	if (0)
	for (tracks::keypoint_frame_iterator it(frame->points.begin()); !it.end(); ++it) {
		pyr_keypoint *k = (pyr_keypoint *) it.elem();

		visual_object::db_keypoint_vector::iterator begin, end, i;
		entry->find(k->cid,begin,end);

		if (begin==end) continue; 

		if (!selected_kpt || selected_kpt == k)
			draw_keypoint(k);

		glPushMatrix();
		glTranslatef(image->width,0,0);
		for (i=begin; i!=end; i++) {
			assert(i->cid == k->cid);
			if (!selected_kpt || selected_kpt->cid == i->cid)
				draw_keypoint(&(*i));

			if (k==selected_kpt && i->node) {
				point2d down(cursor.u, cursor.v+16);
				CvMat mat, rotated; 
				cvInitMatHeader(&mat, patch_tagger::patch_size, patch_tagger::patch_size, CV_32FC1,
						i->node->mean.mean);
				draw_icon(&down, &mat, 16, 16, entry_image->width,0,16);

				cvInitMatHeader(&rotated, patch_tagger::patch_size, patch_tagger::patch_size, CV_32FC1, const_cast<float *>(&i->descriptor._rotated[0][0]));
				draw_icon(&cursor, &rotated, 16, 16, entry_image->width,0,16);
			}
		}
		glPopMatrix();
	}

	for (visual_object::correspondence_vector::iterator it(corresp.begin()); it!= corresp.end(); ++it)
	{
		pyr_keypoint *k = it->frame_kpt;
		pyr_keypoint *o = it->obj_kpt;

		//assert(o->cid == k->cid);

		if (!selected_kpt || selected_kpt == k) {
			draw_keypoint(k);
		}

		glPushMatrix();
		glTranslatef(image->width,0,0);

		if (!selected_kpt || selected_kpt == k) {
			draw_keypoint(o);
		}

		if (k==selected_kpt) {
			if (o->node) {
				point2d down(cursor.u, cursor.v+16);
				CvMat mat; 
				cvInitMatHeader(&mat, patch_tagger::patch_size, patch_tagger::patch_size, CV_32FC1,
						o->node->mean.mean);
				draw_icon(&down, &mat, 16, 16, entry_image->width,0,16);
			}

                        CvMat rotated;
			cvInitMatHeader(&rotated, patch_tagger::patch_size, patch_tagger::patch_size, CV_32FC1, const_cast<float *>(&o->descriptor._rotated[0][0]));
			draw_icon(&cursor, &rotated, 16, 16, entry_image->width,0,16);
		}
		glPopMatrix();
	}
}

void VSView::drawAnnotations(float dx, float dy) 
{
	if (!entry) return;

	QFont font;
	font.setPointSize(12);
	QFontMetrics metric(font);

	int hl = metric.xHeight()/2;

	int text_win_width=0;
	int n=0;
	for (visual_object::annotation_iterator it(entry->annotation_begin()); it!= entry->annotation_end(); ++it, ++n)
	{
		QRect r = metric.boundingRect(QString::fromUtf8(it->descr.c_str()));
		if (r.width() > text_win_width) text_win_width = r.width();
	}

	int text_win_height = (n-1)*metric.lineSpacing() + metric.height();
	int tx = width() - text_win_width - 3*hl;
	int ty = 3*hl;

	float top, bottom, right, left;
	screenToImageCoord(tx-hl,ty-hl, &left, &top);
	screenToImageCoord(tx+hl+text_win_width,ty+hl+text_win_height, &right, &bottom);
	glColor4f(0,0,0,.5);
	glEnable(GL_BLEND);
	glBegin(GL_QUADS);
	glVertex2f(left,top);
	glVertex2f(right,top);
	glVertex2f(right,bottom);
	glVertex2f(left,bottom);
	glEnd();

	glEnable(GL_LINE_SMOOTH);

	n=0;
	for (visual_object::annotation_iterator it(entry->annotation_begin()); it!= entry->annotation_end(); ++it, ++n)
	{
		glColor4f(1,1,1,1);
		int y = ty + n*metric.lineSpacing() + metric.ascent();
		renderText(tx, y, QString::fromUtf8(it->descr.c_str()), font);

		float txim, tyim;
		screenToImageCoord(tx,y-hl , &txim, &tyim);

		glBegin(GL_LINES);
		glVertex2f(txim,tyim);
		glVertex2f(it->x+dx, it->y+dy);
		glEnd();

	}
}

void VSView::paintGL() 
{
	TaskTimer::pushTask("Display");

#ifdef WITH_SIFTGPU
	glDisable(GL_TEXTURE_RECTANGLE_ARB);
#endif
	icon_texture_available.splice(icon_texture_available.begin(), icon_texture_used);

	glDisable(GL_DEPTH_TEST);
	resizeGL(width(),height());

	if (dark) glColor4f(.5,.5,.5,1);
	else glColor4f(1,1,1,1);
	GLBox::paintGL();

	if (tracker==0) {
		TaskTimer::popTask();
		return;
	}

	pyr_frame *frame = (pyr_frame *) tracker->get_nth_frame(0);
	if (!frame) {
		TaskTimer::popTask();
		return;
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	setAxisUp(false);
	setImageSpace();

	draw_entry_image();

	if (view_mode == VIEW_KEYPOINTS) {
		draw_keypoints(frame);
	} else if (view_mode== VIEW_TRACKS) {
		draw_all_tracks(frame);
	} else if (view_mode != VIEW_ANNOTATIONS) {
		if (entry && entry->get_total() >0)
			draw_matches(frame);
		else {
			if (!selected_kpt) {
				draw_keypoints(frame);
				//draw_all_tracks(frame);
			}
		}

		if (selected_kpt) {
			draw_selected_track();
		}
	} 

	drawAnnotations(frame->pyr->images[0]->width, 0);

	if (add_frame_tex.getIm()) {
	       int t = add_frame_time.elapsed();
	       if (t>0 && t< 1200) {
		       glColor4f(1,1,1,1);
		       add_frame_tex.drawQuad(10,10,256,64);
	       }
	}
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
#ifdef WITH_SIFTGPU
	glEnable(GL_TEXTURE_RECTANGLE_ARB);
#endif
	TaskTimer::popTask();
}

void VSView::write_descriptor(pyr_keypoint *k, long ptr)
{
	static const unsigned descr_size=kmean_tree::descriptor_size;
	if ((sizeof(long)+descr_size*sizeof(float)) != sizeof(kmean_tree::descr_file_packet)) {
		cerr << "wrong structure size!!\n";
		exit(-1);
	}

#ifdef WITH_SURF
	if (k->surf_descriptor.descriptor[0]==-1) return;
	for (unsigned i=0;i<64;i++) {
		if (!finite(k->surf_descriptor.descriptor[i])) {
			cerr << "surf descr[" << i << "] = " << k->surf_descriptor.descriptor[i] << endl;
			return;
		}
	}

	if (fwrite(&ptr, sizeof(long), 1, descrf)!=1 ||
			fwrite(k->surf_descriptor.descriptor, 64*sizeof(float), 1, descrf)!=1) {
		cerr << "write error!\n";
		return;
	}
#endif
#ifdef WITH_SIFTGPU
	if (fwrite(&ptr, sizeof(long), 1, descrf)!=1 ||
			fwrite(k->sift_descriptor.descriptor, 128*sizeof(float), 1, descrf)!=1) {
		cerr << "write error!\n";
		return;
	}
#endif
#ifdef WITH_PATCH_TAGGER_DESCRIPTOR
	if (k->descriptor.total==0) {
		std::cout << "save_descriptors: total==0!\n";
		return;
	}

	 
	size_t n = fwrite(&ptr, sizeof(long), 1, descrf);
	float _array[descr_size];
	k->descriptor.array(_array);
	for (int i = 0; i < descr_size; ++i) {
		assert(finite(_array[i]));
	}

	n = fwrite(_array, descr_size * sizeof(float), 1,descrf);
#endif

}

void VSView::save_tracks() 
{
	if (descrf==0) 
		descrf = fopen(descriptors_fn, "ab");

	pyr_frame *frame = (pyr_frame *) tracker->get_nth_frame(2);
	if (!frame) return;

	for (tracks::keypoint_frame_iterator it(frame->points.begin()); !it.end(); ++it) {
		pyr_keypoint *k = (pyr_keypoint *) it.elem();

		if (k->matches.next!=0 || !k->track_is_longer(4)) continue;

		// a long enough track was lost. Save it.
			
		long lastpos = -1;
		for (tracks::keypoint_match_iterator it(k); !it.end(); --it) {
			pyr_keypoint *p = (pyr_keypoint *)it.elem();

			long pos = ftell(descrf);
			write_descriptor(p,lastpos);
			lastpos=pos;
		}
	}
}

void VSView::save_descriptors(pyr_frame *frame)
{
	if (!frame) return;

	if (descrf==0) 
		descrf = fopen(descriptors_fn, "ab");
	
	for (tracks::keypoint_frame_iterator it(frame->points.begin()); !it.end(); ++it) {
		pyr_keypoint *k = (pyr_keypoint *) it.elem();

		//if (!k->track_is_longer(1)) continue;
		write_descriptor(k,-1);
	}
}

void VSView::cmp_affinity() {

	pyr_frame *frame = (pyr_frame *) tracker->get_nth_frame(0);
	for (tracks::keypoint_frame_iterator it(frame->points.begin()); !it.end(); ++it) {
		pyr_keypoint *k = (pyr_keypoint *) it.elem();

		if (k->matches.prev) {
			id_pair pair(k->id,((pyr_keypoint *)k->matches.prev)->id);
			map<id_pair,unsigned>::iterator it = affinity_matrix.find(pair);
			if (it==affinity_matrix.end())
				affinity_matrix[pair]=1;
			else
				it->second++;
		}
	}
}

struct upair {
	unsigned good, bad;
	upair() : good(0),bad(0) {}
};


void VSView::print_affinity() {
	map<unsigned, upair> total;
	for (map<id_pair,unsigned>::iterator it = affinity_matrix.begin();
			it != affinity_matrix.end(); ++it)
	{
		cout << it->first.a << "," << it->first.b << ": " << it->second << endl;

		if (it->first.a == it->first.b) {
			total[it->first.a].good += it->second;
		} else {
			total[it->first.a].bad += it->second;
			total[it->first.b].bad += it->second;
		}
	}

	cout << "total:\n";
	for (map<unsigned, upair>::iterator it=total.begin(); it!=total.end(); ++it)
	{
		cout << "id: " << it->first << " has " << 100.0f*(float)it->second.good/(it->second.good+it->second.bad) << "% success rate, detected  " << it->second.good+it->second.bad  << " times.\n";
	}
}

void VSView::show_track(pyr_keypoint *k)
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
					if (c) {
						cout << "Best cluster: " << c->id << " score= " << d << endl;
					}
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

void VSView::show_tracks()
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

void trim(string& str)
{
	const char* spaces = " \r\f\n\t\v"; 
	string::size_type pos = str.find_last_not_of(spaces);
	if(pos != string::npos) {
		str.erase(pos + 1);
		pos = str.find_first_not_of(spaces);
		if(pos != string::npos) str.erase(0, pos);
	}
	else str.erase(str.begin(), str.end());
}
	
bool VSView::load_script(const char *fn)
{
	ifstream f(fn);

	if (!f.good()) return false;

	while (f.good()) {
		script_instruction instr;

		string s_id, s_instr;
		f >> s_id;
		f >> s_instr;
		getline(f, instr.arg);
		trim(instr.arg);
		if (!f.good()) break;

		if (s_id.length() <1 || s_instr.length() < 1) break;

		int id = atoi(s_id.c_str()); 

		if (s_instr.compare("add")==0) {
			instr.instr = INSTR_ADD;
		} else if (s_instr.compare("addH")==0) {
			instr.instr = INSTR_ADDH;
		} else if (s_instr.compare("addF")==0) {
			instr.instr = INSTR_ADDF;
		} else if (s_instr.compare("echo")==0) {
			instr.instr = INSTR_ECHO;
		}else if (s_instr.compare("check")==0) {
			instr.instr = INSTR_CHECK;
		} else {
			cerr << fn << ": unknown instruction: " << s_instr;
			continue;
		}
		script[id].push_back(instr);
	}
	return true;
}

void VSView::summary()
{

	TaskTimer::printStats();

	cout <<"FPS vs number of features:\n";
	for (fps_stat_map::iterator it(fps_stat.begin()); it!=fps_stat.end(); ++it)
	{
		cout << it->first*fps_stat_bin_size << ".." << ((1+it->first)*fps_stat_bin_size)-1 << ": " 
			<< it->second.first << " ms (count=" << it->second.second << ")\n";
	}
	cout << endl;

	float nframes = (gt_success+gt_fail);
	cout << gt_success << " successfull frames (" << true_neg << " true neg), " << gt_fail << " wrong ones. Ratio: " <<
		(float)gt_success / nframes << endl;
	cout << "false pos: " << false_pos << ", false neg: " << false_neg << endl;
	float sec = total_time.elapsed() / 1000.0f;
	cout << "Video processed in " << sec << "s, average fps: " << nframes/sec << endl;

}

void VSView::script_exec(int id)
{
	if ((entry == 0 && (ground_truth.compare("none")==0 || ground_truth.length()==0))
			|| (entry && ground_truth.compare(entry->get_comment())==0))
	{
		gt_success++;
		if (entry==0) true_neg++;
	} else {
		//cout << "Retrieved: " << (entry ? entry->get_comment() : "none") << ", ground truth: " << ground_truth << endl;
		gt_fail++;
		if (entry==0 && !(ground_truth.compare("none")==0 || ground_truth.length()==0))
			false_neg++;
		if (entry && (ground_truth.compare("none")==0 || ground_truth.length()==0))
			false_pos++;
	}

	script_map::iterator frame_it = script.find(id);
	if (frame_it == script.end()) return;
	for (instr_list::iterator it(frame_it->second.begin()); it!= frame_it->second.end(); ++it)
	{
		switch (it->instr) {
			case INSTR_ECHO: cout << id << ": " << it->arg << endl; break;
			case INSTR_ADD: add_current_frame_to_db(it->arg.c_str(), 0); break;
			case INSTR_ADDH: add_current_frame_to_db(it->arg.c_str(), visual_object::VERIFY_HOMOGRAPHY); break;
			case INSTR_ADDF: add_current_frame_to_db(it->arg.c_str(), visual_object::VERIFY_FMAT); break;
			case INSTR_CHECK: ground_truth = it->arg; break;
		}
	}

}

void VSView::addAnnotation(float x, float y)
{
	if (!entry) return;
	bool restart_timer = false;
	if (timer) {
		restart_timer=true;
		killTimer(timer); 
	}
	timer=0;

	bool ok;
	QString text = QInputDialog::getText(this, "Annotation",
			"Message:", QLineEdit::Normal, "", &ok);
	if (ok && !text.isEmpty()) {
		entry->add_annotation(x,y, 0, (const char *) text.toUtf8());
	}

	if ( restart_timer ) { 
		timer = startTimer(0);
	}
}

