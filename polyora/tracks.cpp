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
#include "tracks.h"
#include <assert.h>

//using namespace std;

const tkeypoint &tkeypoint::operator = (const tkeypoint &a)
{ 
	point2d::operator=(a);
	frame=0;
	track=0;
	points_in_frame.prev=0;
	points_in_frame.next=0;
	matches.prev=0;
	matches.next=0;
	return *this;
}

void tracks::set_match(tkeypoint *prev, tkeypoint *next) {
	assert(prev->matches.next == 0);
	assert(next->matches.prev == 0);
	assert(next->track==0);

	prev->matches.next = next;
	next->matches.prev = prev;
	if (prev->track) {
		next->track = prev->track;
		prev->track->keypoints = next;
	} else {
		ttrack *track = ttrack_factory->create(this);
		MLIST_INSERT(all_tracks, track, track_node)
		track->keypoints = next;
		next->track = track;
		prev->track = track;

		next->track->point_added(prev);
	}
	next->track->point_added(next);
}

void tracks::unset_match(tkeypoint *k) {
	assert(k->matches.prev != 0);
	assert(k->track);

	k->track->point_removed(k);

	RLIST_RM(&k->track->keypoints, k, matches);

	if (k->track->keypoints==0) {
		MLIST_RM(&all_tracks, k->track, track_node);
		ttrack_factory->destroy(k->track);
	}

	k->track=0;
}

void tframe::append_to(tracks &t)
{
	assert(frames.next ==0 && frames.prev==0);
	MLIST_INSERT(t.frames, this, frames);
}

void tkeypoint::set(tframe *f, float u, float v) 
{
	if (frame) unlink();
	frame = f;
	this->u=u;
	this->v=v;
	track=0;
	f->points.add_pt(this);
}

void tkeypoint::unlink() {
	if (frame) frame->points.rm_pt(this);

	if (track) track->point_removed(this);

	RLIST_RM((track?&track->keypoints:0), this, matches);
	if (track&&track->keypoints==0) {
		tracks *db = track->db;
		MLIST_RM(&db->all_tracks,track,track_node);
		db->ttrack_factory->destroy(track);
	}
	track=0;
	frame=0;
}

tkeypoint::~tkeypoint() {
	unlink();
}

int tkeypoint::track_length() {
	int i=0;
	for(tkeypoint *p=matches.prev; p; p=p->matches.prev) i++;
	return i;
}

tracks::tracks(tkeypoint::factory_t *kf, tframe::factory_t *ff, ttrack::factory_t *tf) 
	: frames(0),all_tracks(0) 
{
	static tkeypoint::factory_t default_kf;
	static tframe::factory_t default_ff;
	static ttrack::factory_t default_tf;

	keypoint_factory = (kf !=0 ? kf : &default_kf);
	tframe_factory = (ff !=0 ? ff : &default_ff);
	ttrack_factory = (tf !=0 ? tf : &default_tf);
}

tracks::~tracks() {
	while (frames) {
		frame_iterator it(frames);
		remove_frame(it);
	}
}

tracks::frame_iterator tracks::get_nth_frame_it(int n) {
	frame_iterator it(this);
	for (int i=0; i<n; i++) {
		if (!it.end()) ++it;
		else return it;
	}
	return it;
}

tframe *tracks::get_nth_frame(int n) {
	frame_iterator it = get_nth_frame_it(n);
	return it.frame;
}

void tracks::remove_point_track(tkeypoint *point) {
	if (!point) return;

	tkeypoint *p=point;
	tkeypoint *prevs = point->matches.prev;
	while (p) {
		tkeypoint *np = p->matches.next;
		p->dispose();
		p = np;
	}
	while (prevs) {
		tkeypoint *pp = prevs->matches.prev;
		prevs->dispose();
		prevs = pp;
	}
}
void tracks::remove_track_tail(tkeypoint *point) {
	tkeypoint *prev = point->matches.prev;
	point->dispose();
	if (prev) remove_point_track(prev);
}

void tracks::remove_unmatched_tracks(frame_iterator frame_it) {


	//for (frame_iterator f = frame_it; !f.end(); ++f) 
	if (!frame_it.end())
	{

		bucket2d<tkeypoint>::iterator it(frame_it.frame->points.begin()); 

		while (!it.end()) {
			tkeypoint *kpt = it.elem();
			++it;
			if (!kpt->matches.next) {
				remove_point_track(kpt);
			}
		}
		it=frame_it.frame->points.begin(); 
	}
	remove_empty_frames();
}
		
void tracks::remove_empty_frames() {
	
	frame_iterator it(this); 
	while (!it.end()) {
		frame_iterator next(it); 
		++next;
		if (it.frame->points.size()==0) {
			remove_frame(it);
		}
		it = next;
	}
}

void tracks::remove_frame(frame_iterator &f) {
	if (f.end()) return;
	remove_frame(f.frame);
	f.frame = 0;
}

void tracks::remove_frame(tframe *frame) {

	if (frame==0) return;

	bucket2d<tkeypoint>::iterator it(frame->points.begin()); 

	while (!it.end()) {
		tkeypoint *kpt = it.elem();
		++it;
		remove_track_tail(kpt);
	}

	assert(frame->points.size() == 0);

	MLIST_RM(&frames, frame, frames);

	tframe_factory->destroy(frame);
}

bool tframe::has_point_in(float u, float v, float r) 
{
	float r2 = r*r;
	for (bucket2d<tkeypoint>::iterator it = points.search(u,v,r);
			!it.end(); ++it)
	{
		float du = u-it.elem()->u;
		float dv = v-it.elem()->v;
		if (du*du+dv*dv < r2) return true;
	}
	return false;
}


#if 0
#include <iostream>
void::tracks::print_stats()
{
	int i=0;
	for (frame_iterator it(this); !it.end(); ++it, ++i) {
		std::cout << "Frame " << i << ": " << it.frame->points.size() << " points.\n";
		for (keypoint_frame_iterator pit(it.frame->points.begin()); !pit.end(); ++pit) {
			std::cout << "\t" << pit.elem() << "(p:" << pit.elem()->matches.prev << ",n:" << pit.elem()->matches.next << ") ";
			tkeypoint *k=pit.elem();
			while(k->matches.prev) k=k->matches.prev;
			while(k) {
				std::cout << k << ",";
				k = k->matches.next;
			}
			std::cout << ")\n";
		}
	}
	if (i==0) std::cout << "no frames\n";
}
#endif
