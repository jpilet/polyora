/*
  Copyright 2005, 2006 Computer Vision Lab,
  Ecole Polytechnique Federale de Lausanne (EPFL), Switzerland.
  All rights reserved.

  This file is part of BazAR.

  BazAR is free software; you can redistribute it and/or modify it under the
  terms of the GNU General Public License as published by the Free Software
  Foundation; either version 2 of the License, or (at your option) any later
  version.

  BazAR is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
  PARTICULAR PURPOSE. See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along with
  BazAR; if not, write to the Free Software Foundation, Inc., 51 Franklin
  Street, Fifth Floor, Boston, MA 02110-1301, USA
*/
#ifndef KEYPOINT_H
#define KEYPOINT_H

#ifndef SKIP_OPENCV
#include <opencv/cv.h>
#endif

/* Feature points detection
 */
//@{

/*!
  \brief A 2D feature point as detected by a point detector (\ref yape for example).
*/
class keypoint
{
 public:
   keypoint(void) { u=-1; v=-1; scale=0; id=-1; }
   keypoint(float u, float v, float scale) { this->u = u; this->v = v; this->scale = scale; id=-1; }
   keypoint(const keypoint & p) { u = p.u; v = p.v; scale = p.scale; score = p.score; id=p.id; }
   virtual ~keypoint() {}

   keypoint & operator =(const keypoint &p) { u = p.u; v = p.v; scale = p.scale; score = p.score; id=p.id; return *this; }

  //@{
  float u, v;  //!< 2D coordinates of the point (at the pyramid level 'scale')
  float u_undist, v_undist; //!< normalized coordinates, at pyramid level 0.

  int scale; //!< point scale
  float score; //!< higher is the score, stronger is the point

  int class_index; //!< used in fern_based_point_classifier
  float class_score;

  float fr_u(void) { int s = int(scale);  int K = 1 << s;  return K * u; }
  float fr_v(void) { int s = int(scale);  int K = 1 << s;  return K * v; }
  //@}

  //! Use \ref keypoint_orientation_corrector to compute keypoint orientation:
  float orientation ; //!< Between 0 and 2 Pi

  //@{
  //! Used for point matching, should disappear soon
  float meanI, sigmaI;
  keypoint * potential_correspondent;
  float match_score;
  int i_bucket, j_bucket;

  int index; // used to locate the keypoint in the keypoint array after matching.
  //@}

   int id;

};

#ifndef SKIP_OPENCV
/*! \file */

/*!
  \fn CvPoint mcvPoint(keypoint & p)
  \brief Convert a keypoint into a cvPoint from OpenCV
  \param p the point
*/
inline CvPoint mcvPoint(keypoint & p)
{
  int s = int(p.scale);
  int K = 1 << s;
  return cvPoint(int(K * p.u + 0.5), int(K * p.v + 0.5));
}
#endif
//@}
#endif // KEYPOINT_H
