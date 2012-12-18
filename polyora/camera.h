#ifndef _CAMERA_H
#define _CAMERA_H

#include "matvec.h"
#include <vector>

/*!
 * \brief This class represents a calibrated camera.
 * \ingroup geomcalib
 *
 * The camera space is right-handed, i.e.: 
 *     x -> points right
 *     y -> points up
 *     z -> points away through screen
 *
 * The image space origin is bottom-left, and the y axis points up.
 * 
 * For square pixels s should be 0 and f==g.
 *
 * Projection equation of the 3D point P:
 *
 * \f[u = \frac{(f * Px + s * Py)}{Pz + cx}\f]
 *
 * \f[v = \frac{g * Py}{Pz + cy}\f]
 */
class PerspectiveProjection {
public:
  PerspectiveProjection();
  virtual ~PerspectiveProjection(){}

  PerspectiveProjection(int w, int h, double f, double g, double cx, double, double s=0);
  void set(int w, int h, double f, double g, double cx , double cy, double s=0);
  bool loadOpenCVCalib(const char* filename);

  void eyeToImage(const double eye[3], double uv[2]) const;
  void imageToEye(const double uv[2], double eye[3], double w=0) const;
  void imageToEye(double u, double v, double eye[3], double w=0) const;

  void getGlProjection(double proj[4][4]) const;
  void getD3DProjection(double proj[4][4]) const;
  // void setD3DProjection(const double proj[4][4]) const;

  bool setPlanes(double near, double far);

  // Return a matrix containing:
  // [ f 0 cx ]
  // [ 0 g cy ]
  // [ 0 0  1 ]
  cv::Mat getIntrinsics() const;

  bool getUndistortMap(CvMat *xmap, CvMat *ymap);

  //protected:
  // intrinsic parameters
  double f, g, cx, cy, s;
  cv::Mat distortion;
  Mat3x3 eyeToImageMat; /* ~ Projection Matrix in Direct3D */
  int width, height;
  double farPlane, nearPlane;

  void cmpEyeToImageMat();

  virtual void flip();
  virtual void resizeImage(int newW, int newH);
};

/*! Perspective camera: a projection located in space.
 */
class PerspectiveCamera : public PerspectiveProjection {
public:
  PerspectiveCamera();

  void worldToImage(const double p[3], double uvw[3]) const;
  void worldToImage(const Vec3 &p, Vec3 &uvw) const;
  void worldToEye(const Vec3 &src, Vec3 &dst) const;
  void worldToEye(const double src[3], double dst[3]) const;
  void eyeToWorld(const Vec3 &uvw, Vec3 &w) const;
  void imageToWorld(double u, double v, Vec3 &w, double z=0) const;
  void getGlModelView(double view[4][4]) const;
  void getDirect3DView(double view[4][4]) const;
  // void setDirect3DView(const double view[4][4]) const;
  void setByTarget(const Vec3 pos, const Vec3 target, double roll);
  void loadTdir(const double tdir[3][4], int w, int h);
  bool loadTdir(const char *tdir, int w, int h);
  bool loadTdir(const char *tdir); // width and height are estimated. Do not trust it!
  void clearExternalParams();
  void getExternalParameters(double& tx, double &ty, double &tz, double &omega, double &phi, double &kappa);
  void RotationMatrixFromOmegaPhiKappa(Mat3x3 &rotation, const Vec3 &angles);
  void GetEulerAnglesFromRotation(const Mat3x3 &rotationTrans, Vec3 &angles );

  //! translate the camera relative to eye coordinates (z means "move forward").
  void translate(double dx, double dy, double dz);

  void setWorldToEyeMat(const Mat3x4 &m);
  const Mat3x4 &getWorldToEyeMat() const { return worldToEyeMat; }
  const Mat3x4 &getEyeToWorldMat() const { return eyeToWorldMat; }
  const Mat3x4 &getWorldToImageMat() const { return worldToImageMat; }

  // Returns the output of cv::Rodrigues
  cv::Mat getExpMapRotation() const;

  cv::Mat getTranslation() const;
  void setTranslation(const cv::Mat &translation);

  // Uses cv::Rodrigues to compute the rotation matrix
  void setExpMapRotation(const cv::Mat &rotation_params);

  void cmpWorldToImageMat();

  //! Save the camera in a tdir file. Returns true on success, false on failure.
  bool saveTdir(const char *file);

  virtual void flip(); 
  virtual void resizeImage(int newW, int newH);

  void setPoseFromHomography(const double H[3][3]);
  void setPoseFromHomography(const float H[3][3]);

protected:
  // extrinsic parameters
  Mat3x4 worldToEyeMat; /* ~ View Matrix in Direct3D */
  Mat3x4 eyeToWorldMat; 
  Mat3x4 worldToImageMat;
};

std::ostream& operator << (std::ostream& os, const PerspectiveProjection &cam);
std::ostream& operator << (std::ostream& os, const PerspectiveCamera &cam);

#endif

