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
#ifndef VSVIEW_H
#define VSVIEW_H

#include <videosource.h>
#include <cv.h>
#include <qapplication.h>

#include <QKeyEvent>
#include <QTimerEvent>
#include <QTime>

#include "glbox.h"
#include "ipltexture.h"
#include <polyora/polyora.h>

class VSView : public GLBox {
Q_OBJECT

public:
	VSView(QWidget *parent, const char *name, VideoSource *vs);
	~VSView();
	VideoSource *vs;

	void saveCurrentFrame(const char *filename);
	
public slots:
	void timerEvent( QTimerEvent *);
	void keyPressEvent(QKeyEvent *k);
	void mousePressEvent ( QMouseEvent * event );
	void mouseReleaseEvent ( QMouseEvent * event );
	void mouseDoubleClickEvent ( QMouseEvent * event );
	void paintGL();
	void show_help();

private:
	// idle timer
	int timer;
	bool filter;
	bool viewScores;

public:
	bool record;
	bool record_pts;
	bool record_movie;
	bool auto_index;
	bool dark;

private:

	int nbLev;
	int viewlevel;

	IplImage *im;

	kpt_tracker *tracker;
	visual_database database;
	incremental_query *query;

	visual_object::correspondence_vector corresp;

	// fps counter
	QTime qtime;
	int frameCnt;
	int frameno;

	FILE *descrf;

public:
	const char *tree_fn, *clusters_fn, *descriptors_fn;
	const char *visual_db_fn;
	float threshold;
	int query_flags;
private:

	std::list<IplTexture> icon_texture_used, icon_texture_available;

	IplTexture entry_tex;
	IplTexture add_frame_tex;
	QTime add_frame_time;

	visual_object *entry;
	int nb_missing_frames;
	pyr_keypoint *selected_kpt;

	void save_descriptors(pyr_frame *frame);
	bool selectKeypoint (QMouseEvent *event);
	void cmp_affinity();
	void print_affinity();
	void show_track(pyr_keypoint *k);
	void show_tracks();
	void write_descriptor(pyr_keypoint *k, long ptr);
	void save_tracks();
	void createTracker();
	void draw_matches(pyr_frame *frame);
	void draw_keypoints(pyr_frame *frame);
	void draw_all_tracks(pyr_frame *frame);
	void draw_selected_track();
	void draw_icon(point2d *c, const CvArr *image, float w, float h, float max_width, int margin_x=0, int margin_y=0);
	void draw_entry_image();
	void drawAnnotations(float dx, float dy);
	void addAnnotation(float x, float y);
	void add_current_frame_to_db(const char *name, int flags);

	enum view_mode_t { VIEW_AUTO, VIEW_KEYPOINTS, VIEW_TRACKS, VIEW_ANNOTATIONS };
	view_mode_t view_mode;

	// scripting stuff
	enum instr_t { INSTR_ECHO, INSTR_ADD, INSTR_ADDH, INSTR_ADDF, INSTR_CHECK };
	struct script_instruction {
		instr_t instr;
		std::string arg;
	};

	typedef std::list< script_instruction > instr_list;
	typedef std::map<int, instr_list > script_map;
	script_map script;
	int gt_success, gt_fail, false_pos, false_neg, true_neg;
	QTime total_time;
	std::string ground_truth;

	typedef std::map<unsigned, std::pair<double, unsigned> > fps_stat_map;
	fps_stat_map fps_stat;
	void update_fps_stat(float fr_ms, pyr_frame *pframe);

public:
	bool load_script(const char *fn);
	void summary();
protected:
	void script_exec(int id);
	void segment_scene();
	int nlost;
	QWidget *help_window;
public:
	bool use_pipeline;
};

#endif

