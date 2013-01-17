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

#include "vobj_tracker_view.h"
#include <QDir>
#include <QInputDialog>
#include <map>
#include <polyora/timer.h>
#include <math.h>
#include <QTextEdit>

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
vobj_tracker_view::vobj_tracker_view(QWidget *parent, const char *name, VideoSource *vs)
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

	help_window=0;

	//query_flags = incremental_query::QUERY_IDF|incremental_query::QUERY_NORMALIZED_FREQ;

	nb_missing_frames=0;
	auto_index = false;

	nbLev = 10;
	tracker=0;
	frameCnt=0;
	frameno=0;
	threshold = .06;
	im=im2=0;

	draw_flags = DRAW_COLOR | DRAW_SCRIPT;

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

/*
template <class T> void swap(T &a, T &b)
{
	T c(a);
	a=b;
	b=c;
}
*/

vobj_tracker_view::~vobj_tracker_view() {

	if (im) cvReleaseImage(&im);
	if (im2) cvReleaseImage(&im2);
	if (vs) delete vs;
	if (tracker) delete tracker;
	if (help_window) delete help_window;
}

void vobj_tracker_view::timerEvent(QTimerEvent *) {


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
		vobj_frame *pframe = static_cast<vobj_frame *>(tracker->process_frame_pipeline(frame, vs->getId()));

		if (pframe) {
			setImage(pframe->pyr->images[0]);
			pframe->timestamp = time_since_started.elapsed();
		}


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

		script_engine.processFrame(pframe);
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

void vobj_tracker_view::update_fps_stat(float fr_ms, pyr_frame *pframe)
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


void vobj_tracker_view::saveCurrentFrame(const char *filename)
{
	cvSaveImage(filename, image);
}

static inline bool toggle(unsigned &flags, unsigned bit) { return flags = flags ^ bit; }

static const char *help =
"Key mapping:\n"
"--- Database editing ---\n"
"h - add current view as a new planar object\n"
"backspace - delete visible object(s) from DB\n"
"i - toggle incremental learning\n"
"\n--- Video/Processing control ---\n"
"tab - pause/resume video input\n"
"space - pause/resume processing\n"
"\n--- Display ---\n"
"f: toggle fullscreen display\n"
"1: Display key points\n"
"2: Display tracks\n"
"3: Show matched points\n"
"4: Show augmented reality result\n"
"g: toggle augmented reality display\n"
"c: toggles gray/color display\n"
"d: toggles image darkening for better constrast\n"
"m: show/hide matches\n"
"\n--- Miscellaneous ---\n"
"s: save a screen shot as 'shot0000.png'\n"
"q: quit the program\n"
"z: reset view\n";

void vobj_tracker_view::show_help()
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

void vobj_tracker_view::keyPressEvent(QKeyEvent *k)
{
	static int n=0;
	char fn[256];
	vobj_frame *frame = (vobj_frame *) tracker->get_nth_frame(0);

	switch (k->key()) {
		case Qt::Key_1: draw_flags = DRAW_COLOR|DRAW_TRACKS; break;
		case Qt::Key_2: draw_flags = DRAW_COLOR|DRAW_KEYPOINTS|DRAW_DARK; break;
		case Qt::Key_3: draw_flags = DRAW_COLOR|DRAW_MATCHES|DRAW_DARK; break;
		case Qt::Key_4: draw_flags = DRAW_COLOR|DRAW_INSTANCE; break;
		case Qt::Key_5: draw_flags = DRAW_COLOR|DRAW_SCRIPT; break;
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
		case Qt::Key_L: script_engine.runScript("polyora.js"); break;
		//case Qt::Key_F: add_current_frame_to_db("Interactive, F-Mat checking", visual_object::VERIFY_FMAT); break;
		case Qt::Key_H: add_current_frame_to_db("Interactive, homography checking", visual_object::VERIFY_HOMOGRAPHY); break;
		case Qt::Key_Plus: threshold += .005; cout << "T=" << threshold << endl; break;
		case Qt::Key_Minus: threshold -= .005; cout << "T=" << threshold << endl; break;
		case Qt::Key_Z: zoom(1,1); translate(0,0); resize(frame->pyr->images[0]->width, frame->pyr->images[0]->height); break;
		default: if ((k->key()>= Qt::Key_F1 && k->key()<=Qt::Key_F12) || k->text().size() > 0)
				 show_help();
	}
	update();
}



void vobj_tracker_view::add_current_frame_to_db(const char *name, int flags)
{
	pyr_frame *frame = (pyr_frame *)tracker->get_nth_frame(0);

	cout << "Adding object " << name << endl;
	visual_object *obj = database.create_object(name, flags);
	obj->add_frame(frame);
	obj->prepare();
	database.add_to_index(obj);
	add_frame_time.restart();
	char filename[256];
	sprintf(filename, "object%03d.png", static_cast<int>(obj->id()));
	cvSaveImage(filename, image);
}

bool vobj_tracker_view::selectKeypoint(QMouseEvent *event)
{
	point2d c;
	screenToImageCoord(event->x(), event->y(), &c.u, &c.v);

	pyr_frame *frame = (pyr_frame *) tracker->get_nth_frame(0);

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

void vobj_tracker_view::mouseDoubleClickEvent ( QMouseEvent * ) {
	selected_kpt=0;
	update();
}

void vobj_tracker_view::mousePressEvent ( QMouseEvent * event )
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

void vobj_tracker_view::mouseReleaseEvent ( QMouseEvent * event )
{
	GLBox::mouseReleaseEvent(event);
}

void draw_keypoint(pyr_keypoint *k) {
	float x = k->u;
	float y = k->v;
	float angle = k->descriptor.orientation;
	float len=5 +   (2 << k->scale);
	float dx = len * cosf(angle);
	float dy = len * sinf(angle);

	if (k->cid) {
		gl_hash_mark(k->cid, k->u, k->v, k->descriptor.orientation, len);
	} else {
		glBegin(GL_LINES);
		glVertex2f(x,y);
		glVertex2f(x+dx,y+dy);
		glVertex2f(x-dy/4,y+dx/4);
		glVertex2f(x+dy/4,y-dx/4);
		glEnd();
	}
}

void vobj_tracker_view::createTracker()
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
		script_engine.setup(this, &database, 0, texture);
		if (!script_engine.runScript("polyora.js")) {
      draw_flags = DRAW_COLOR | DRAW_INSTANCE;
    }

		cout << "Ready. Press F1 for help.\n";
		time_since_started.start();
	}
}

static void HSVtoRGB( float *r, float *g, float *b, float h, float s, float v )
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

static void gl_hash_color(unsigned key)
{
	float c[3];
	HSVtoRGB(c,c+1,c+2, fmod((float)key, 360), 1, 1);
	glColor3fv(c);
}

void gl_hash_mark(unsigned code, float x, float y, float orient, float r)
{
	//gl_hash_color(code);

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

void vobj_tracker_view::draw_selected_track()
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
#ifndef WITH_SURF
		pyr_keypoint *k = (pyr_keypoint *) it.elem();

		if (k->node) {
			CvMat mat; cvInitMatHeader(&mat, patch_tagger::patch_size, patch_tagger::patch_size, CV_32FC1,
					k->node->mean.mean);
			point2d pos_down(pos.u, pos.v+r);
			draw_icon(&pos_down, &mat, r, r, image->width, 0, r);
		}
                CvMat rotated;
                cvInitMatHeader(&rotated, patch_tagger::patch_size, patch_tagger::patch_size, CV_32FC1,
                        k->descriptor._rotated);
		draw_icon(&pos, &rotated, r, r, image->width, 0, r);
#endif
	}
	glPopMatrix();
	glPopMatrix();
}

void vobj_tracker_view::draw_icon(point2d *c, CvArr *image, float w, float h, float max_width, int margin_x, int margin_y)
{
	if (!image) return;

	if (icon_texture_available.begin() == icon_texture_available.end()) {
		IplTexture t;
		icon_texture_available.push_back(t);
	}
	icon_texture_used.splice(icon_texture_used.begin(), icon_texture_available, icon_texture_available.begin());
	IplTexture &t(icon_texture_used.front());
	CvMat m;
	t.setImage( cvGetMat(image, &m));
	glColor4f(1,1,1,1);
	t.drawQuad(c->u, c->v, w, h);
	c->u +=  w + margin_x;
	if (c->u>=max_width) {
		c->u = 0;
		c->v += h + margin_y;
	}
}

void vobj_tracker_view::draw_all_tracks(pyr_frame *frame)
{
	glColor4f(0,1,0,1);

	for (tracks::keypoint_frame_iterator it(frame->points.begin()); !it.end(); ++it) {
		pyr_keypoint *k = (pyr_keypoint *) it.elem();

		if (k->track_is_longer(2)) {
			glBegin(GL_LINE_STRIP);

			int n=0;


			for (tracks::keypoint_match_iterator it(k); !it.end(); --it) {
				pyr_keypoint *p = (pyr_keypoint *)it.elem();

				float alpha = 1.0f-(n++)/256.0f;
				glColor4f(0,1,0,alpha);
				glVertex2f(p->u, p->v);
			}
			glEnd();
		}
	}
}

void vobj_tracker_view::draw_keypoints(pyr_frame *frame)
{
	for (tracks::keypoint_frame_iterator it(frame->points.begin()); !it.end(); ++it) {
		pyr_keypoint *k = (pyr_keypoint *) it.elem();

		//if (!k->track_is_longer(5)) continue;

		glColor4f(0,1,0,.5);
		draw_keypoint(k);

	}
}

void vobj_tracker_view::draw_entry_image(visual_object *entry)
{
	if (!entry) return;

	IplImage *entry_image = database.get_image(entry->representative_image);
	entry_tex.setImage(entry_image);

	if (entry_image)
		entry_tex.drawQuad(entry_image->width,0);
}

void vobj_tracker_view::draw_matches(vobj_frame *frame)
{
	for (tracks::keypoint_frame_iterator it(frame->points.begin()); !it.end(); ++it) {
		vobj_keypoint *k = (vobj_keypoint *) it.elem();

		//if (!k->track_is_longer(5)) continue;

		if (k->vobj) {
			vobj_keypoint  *prevk = k->prev_match_vobj();
			if (prevk && prevk->vobj == k->vobj)
				glColor4f(0,1,0,1);
			else
				glColor4f(1,0,0,1);
			gl_hash_mark((int)(((long)k->vobj)&0xFFFFFFFF), k->u, k->v, k->descriptor.orientation, 3);
		}

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

GLuint vobj_tracker_view::get_texture_for_obj(visual_object *obj)
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

void vobj_tracker_view::homography_augment(visual_object *obj, float H[3][3])
{
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	setupHomography(H);

	const int n=9;
	const int skip=2;
	float dx = image->width/n;
	float dy = image->height/n;

	GLuint texture=get_texture_for_obj(obj);
	glEnable(GL_BLEND);
	if (texture) {
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, texture);
		glColor4f(1,1,1,1);
		glBegin(GL_QUADS);
		glTexCoord2f(0,0);
		glVertex2f(skip*dx, skip*dy);
		glTexCoord2f(1,0);
		glVertex2f((n-skip)*dx, skip*dy);
		glTexCoord2f(1,1);
		glVertex2f((n-skip)*dx, (n-skip)*dy);
		glTexCoord2f(0,1);
		glVertex2f(skip*dx, (n-skip)*dy);
		glEnd();
		glDisable(GL_TEXTURE_2D);
	} else {
		glEnable(GL_LINE_SMOOTH);
		glColor4f(1,1,1,1);
		glBegin(GL_LINES);
		for (int i=skip; i<=(n-skip); i++) {
			glVertex2f(i*dx, skip*dy);
			glVertex2f(i*dx, (n-skip)*dy);
			glVertex2f(skip*dx, i*dy);
			glVertex2f((n-skip)*dx, i*dy);
		}
		glEnd();
	}

	glPopMatrix();
}

void vobj_tracker_view::draw_instances(vobj_frame *frame)
{
	for (vobj_instance_vector::iterator it(frame->visible_objects.begin());
			it != frame->visible_objects.end(); ++it)
	{
		if (it->object->get_flags() & visual_object::VERIFY_HOMOGRAPHY) {
			homography_augment(it->object, it->transform);
		}
	}
}

void vobj_tracker_view::paintGL() 
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

	// Display a message to notify the user that a new object has been
	// added to the database. The notification is visible during 1.2 sec.
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

	if ((draw_flags & DRAW_SCRIPT) && script_engine.isReady())
	    script_engine.render();
	
	//draw_keypoints(frame);
	if (draw_flags & DRAW_MATCHES)
		draw_matches(frame);

	if (draw_flags & DRAW_INSTANCE)
		draw_instances(frame);

	if (draw_flags & DRAW_KEYPOINTS)
		draw_keypoints(frame);

	if (draw_flags & DRAW_TRACKS)
		draw_all_tracks(frame);

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
#ifdef WITH_SIFTGPU
	glEnable(GL_TEXTURE_RECTANGLE_ARB);
#endif
	TaskTimer::popTask();
}

void vobj_tracker_view::show_track(pyr_keypoint *k)
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

void vobj_tracker_view::show_tracks()
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

void vobj_tracker_view::summary()
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


