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
#ifndef TRACKS_H
#define TRACKS_H

#include "mlist.h"
#include "bucket2d.h"

/*! \defgroup TracksGroup Sparse point track structures
  These classes and structures are responsible to store efficiently tracks of features.
  It can be seen as a sparse matrix, whose columns are frame and rows are tracks.
  These structures take care of efficient storage only. The actual feature
  detection and matching is done in \ref KptTrackingGroup .

  See class \ref tracks for more details.
*/
/*@{*/

struct tframe;
struct ttrack;
class tracks;

/*! Basic 2-D point, in float.*/
struct point2d {
	float u,v;

	point2d(float u, float v) : u(u), v(v) {}
	point2d() {}

	float dist(const point2d &a);
};

/*! A point integrated in the "tracks" structure.
*/
struct tkeypoint : point2d {

	//! doubly linked list to other points in the same frame
	mlist_elem<tkeypoint> points_in_frame;

	//! pointers to corresponding points on next and previous frames.
	mlist_elem<tkeypoint> matches;

	//! Pointer to the frame the point belongs to. Can't be null.
	tframe *frame;

	/*! A pointer to the track the point belongs to. The pointer is null if
	 * it has never been matched.
	 */
	ttrack *track;

	tkeypoint() { frame=0; track=0; }
	tkeypoint(const tkeypoint &a) : point2d(a), frame(0), track(0) {}

	/*! act as a constructor. Before calling set(), make sur the point is unlink()ed.
	 */
	void set(tframe *f, float u, float v); 
	const tkeypoint &operator = (const tkeypoint &a);

	//! return the length of the track the point belongs to.
	int track_length();

	//! return true if the point has been matched on at least 'l' frames.
        bool track_is_longer(int l);

	//! overload this function to handle forgotten keypoints. Allows memory recycling.
	virtual void dispose() { delete this; }
	virtual ~tkeypoint();

	//! Remove the point from the tracks structure.
	/*! This method removes the point from the match linked list and from
	 * the frame point list. 
	 */
	void unlink();

	/*! A tkeypoint factory. 
	 * Usefull for inserting objects that derive from tkeypoint into a
	 * tracks structure. 
	 */
	struct factory_t {
		virtual tkeypoint *create() { return new tkeypoint(); }
		virtual void destroy(tkeypoint *a) { delete a; } 
	};
};

class tracks;

/*! A frame in the tracks structure.
 * A frame has a collection of tkeypoints. It also has pointers to previous and
 * next frames. Other data must be stored in derived classes.
 */
struct tframe {
	/*! Collection of tkeypoints.
	 * the bucked2d<> structure allows fast localized random access.
	 */
	bucket2d<tkeypoint> points;

	//! Pointers to next and previous frame.
	mlist_elem<tframe> frames;

	//! Construct a new frame.
	//! The remaining arguments are those of bucket2d<>.
	tframe(int w, int h, int bits) : points(w,h,bits) {}

	//! insert the frame into a "tracks" structure. 
	virtual void append_to(tracks &track);

	//! overload this function to hanlde forgotten frames
	virtual ~tframe() {}

	/*! returns true if at least one point lies in the circle centered at
	 * (u,v) with radius r.
	 */
	bool has_point_in(float u, float v, float r);

	//! a frame factory, useful for deriving the class tframe.
	struct factory_t {
		virtual void destroy(tframe *a) { delete a; } 
	};
};

/*! A ttrack is a set of tkeypoints showing the same physical point on successive frames.
 */
struct ttrack {

	//! pointer to next/previous ttrack.
	mlist_elem<ttrack> track_node;

	/*! pointer to the latest point of the track, i.e. on the latest frame
	 * the point has been seen.
	 * keypoints->matches.next is always null. 
	 */
	tkeypoint *keypoints;

	//! a pointer to the tracks structure the ttrack belongs to.
	tracks *db;

	//! the number of keypoints in the track.
	int length;

	ttrack(tracks *db) : db(db) { keypoints=0; length=0; }
	virtual ~ttrack() {}

	/*! An overloadable method that is called whenever a point is added to the track.
	 * The inserted point is assumed to be yet unmatched, i.e. it does not belong to another track.
	 */
	virtual void point_added(tkeypoint *) { length++; }

	//! An overloadable method that is called whenever a point is removed from the track.
	virtual void point_removed(tkeypoint *) { length--; }

	//! a ttrack factory to let the tracks structure use subclasses of ttrack.
	struct factory_t {
		virtual ttrack *create(tracks *db) { return new ttrack(db); }
		virtual void destroy(ttrack *a) { delete a; } 
	};
};

/*! A structure for storing sparse tracks of points.
 * The tracks class can be viewed as a sparse matrix whose columns are made of
 * frame and whose rows are tracks of points. The matrix is stored in a sparse
 * manner. Access to elements is done through iterators. 
 * Point access can be achieved by:
 * - iterating over frames, then over points of the same frame
 * - iterating over tracks, then over points of the same track
 * - it is also possible, given a frame, a 2d location, and a radius, to
 *   retrieve quickly all the keypoints within this radius.
 *
 * This class provides only storage. The actual tracking is the user's responsability.
 */
class tracks {
public:

        /*! tracks constructor. Takes factories for tkeypoint, tframe and
	 * ttrack as arguments. This way, the user can use derived classes.
	 */
	tracks(tkeypoint::factory_t *kf=0, tframe::factory_t *ff=0, ttrack::factory_t *tf = 0);
	~tracks();
	
	//! Connects "prev" with "next". Both points must be on consecutive frames
	void set_match(tkeypoint *prev, tkeypoint *next);

	//! Disconnects k and k->matches.prev.
	void unset_match(tkeypoint *k);

	//! Frame iterator.
	struct frame_iterator {
		tframe *frame;
		tframe *operator ++() { return frame = frame->frames.next; }
		tframe *operator --() { return frame = frame->frames.prev; }
		bool end() { return frame == 0; }
		frame_iterator(tracks *t) { frame = t->frames; }
		frame_iterator(tframe *f) { frame = f; }
		tframe *elem() { return frame; }
	};

	//! Get a frame iterator for the Nth last frame. n=0 returns the most recent frame.
	frame_iterator get_nth_frame_it(int n);

	//! returns a pointer to the Nth last frame. 0 returns the most recent frame.
	tframe *get_nth_frame(int n);

	//! Spacial iterator for keypoints within a frame.
	typedef bucket2d<tkeypoint>::iterator keypoint_frame_iterator;

	//! Temporal iterator for keypoints with a track.
	struct keypoint_match_iterator {
		tkeypoint *point;
		tkeypoint *operator ++() { return point = point->matches.next; }
		tkeypoint *operator --() { return point = point->matches.prev; }
		tkeypoint &operator ->() { return *point; }
		tkeypoint *elem() { return point; }
		bool end() { return point==0; }
		keypoint_match_iterator(tkeypoint *p) : point(p) {}
	};

	/*! Delete all the keypoints stored in the linked list 'prev_frame'.
	 * Fixes prev_in_frame/next_in_frame linked list.
	 */
	void remove_point_track(tkeypoint *point);

	//! removes "point" and all its previous correspondences.
	void remove_track_tail(tkeypoint *point);

	/*! removes all tracks that do not have a valid point->matches.next pointer.
	 */
	void remove_unmatched_tracks(frame_iterator frame_it);

	//! Removes all frames that do not contain any keypoint.
	void remove_empty_frames();
		
	//! Removes the frame pointed by frame iterator f.
	void remove_frame(frame_iterator &f);

	//! Removes the frame pointed by frame.
	void remove_frame(tframe *frame);

	tframe *frames;
	ttrack *all_tracks;

	tkeypoint::factory_t *keypoint_factory;
	tframe::factory_t *tframe_factory;
	ttrack::factory_t *ttrack_factory;
};

inline float point2d::dist(const point2d &a)
{
	float du = u-a.u;
	float dv = v-a.v;
	return sqrtf(du*du + dv*dv);
}

inline bool tkeypoint::track_is_longer(int l) {
    if (!track) return l==0;
    return track->length >=l;
    /*
                tkeypoint *p=this->matches.prev;
                for(int i=l;i;--i) {
                        if (p==0) return false;
                        p=p->matches.prev;
                }
                return true;
                */
}

/*@}*/

#endif
