#include <fstream>
#include <iostream>
#include <string.h> // for memset
#include "camera.h"
#include "matvec.h"

#include <opencv/cv.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

using namespace std;

PerspectiveProjection::PerspectiveProjection()
{
  set(640,480,1000,1000,320,240,0);
}

PerspectiveProjection::PerspectiveProjection(int w, int h, double f, double g, double cx, double cy, double s)
{
  set(w,h,f,g,cx,cy,s);
}

void PerspectiveProjection::set(int w, int h, double f, double g, double cx, double cy, double s)
{
  this->width = w;
  this->height = h;
  this->f = f;
  this->g = g;
  this->cx = cx;
  this->cy = cy;
  this->s = s;
  distortion = 0;

  nearPlane = f/16;
  farPlane = f*16;
  cmpEyeToImageMat();
}

/*! Set near and far clipping planes. Both values have to be positive. Far
* should also be larger than near. If the values are incorrect, the near and
* far are not modified.
* \return true if near and far are valid and accepted, false otherwise.
*/
bool PerspectiveProjection::setPlanes(double nearPlane, double farPlane)
{
  if ((nearPlane > 0) && (nearPlane < farPlane)) {
    this->nearPlane = nearPlane;
    this->farPlane = farPlane;
    return true;
  }
  return false;
}


/*! Project eye coordinates to image coordinates.
* uv[0] is the horizontal axis (left -> right) and uv[1] is the vertical axis,
* pointing up.
*/ 
void PerspectiveProjection::eyeToImage(const double eye[3], double uv[2]) const
{
  uv[0] = (f * eye[0] + s * eye[1]) / eye[2] + cx;
  uv[1] = (g * eye[1]) / eye[2] + cy;
}

/*! Find the 3D eye coordinate of a given pixel.
* u is the horizontal axis (left -> right) and v is the vertical axis,
* pointing up.
*/ 
void PerspectiveProjection::imageToEye(double u, double v, double eye[3], double w) const
{
  if (w==0) w = nearPlane;

  eye[1] = (v - cy)*(f/g);
  eye[0] = (u - cx - s*eye[1]);
  eye[2] = f;

  //double l = this->nearPlane / sqrt(eye[0]*eye[0] + eye[1]*eye[1] + eye[2]*eye[2]);
  double l = w / f;
  eye[0] *= l;
  eye[1] *= l;
  eye[2] *= l;
}

void PerspectiveProjection::imageToEye(const double uv[2], double eye[3], double w) const
{
  imageToEye(uv[0], uv[1], eye, w);
}

/*!
* This will produce the exact same projection in the case s=0. Otherwise,
* there might be some clipping problems.
*
* The trick is to remember that OpenGL has a negative Z projection.
* You should glScale(1,1,-1); if you use positive Z.
*
* OpenGl, after projection, clips coordinates to [-1, 1] on all axis. It is
* converted then to window coordinates.
*/
void PerspectiveProjection::getGlProjection(double proj[4][4]) const 
{
  memset(proj,0,sizeof(double) * 4 * 4);

  double w = (double) width;
  double h = (double) height;
  
  double near_ = this->nearPlane;
  double far_ = this->farPlane;

  proj[0][0] = 2*f/w;        proj[0][1] = 0;            proj[0][2] = 0;                      proj[0][3] =  0;
  proj[1][0] = 2*s/w;        proj[1][1] = 2*g/h;        proj[1][2] = 0;                      proj[1][3] =  0;
  proj[2][0] = (-2*cx/w +1); proj[2][1] = (-2*cy/h +1); proj[2][2] = -(far_+near_)/(far_-near_); proj[2][3] = -1;
  proj[3][0] = 0;            proj[3][1] = 0;            proj[3][2] = -2*far_*near_/(far_-near_); proj[3][3] =  0;
}

void PerspectiveProjection::getD3DProjection(double proj[4][4]) const
{
  getGlProjection(proj);

  // GL to Direct3D (negative Z projection to positive Z projection)
  proj[2][0] =  -proj[2][0];
  proj[2][1] =  -proj[2][1];
  proj[2][2] =  -proj[2][2];
  proj[2][3] =  -proj[2][3];
}

void PerspectiveProjection::cmpEyeToImageMat()
{
  eyeToImageMat.m[0][0] = f;
  eyeToImageMat.m[1][0] = 0;
  eyeToImageMat.m[2][0] = 0;
  eyeToImageMat.m[0][1] = s;
  eyeToImageMat.m[1][1] = g;
  eyeToImageMat.m[2][1] = 0;
  eyeToImageMat.m[0][2] = cx;
  eyeToImageMat.m[1][2] = cy;
  eyeToImageMat.m[2][2] = 1;
}

/*! Inverse V axis (projected image upside/down)
*/
void PerspectiveProjection::flip()
{
  g= -g;
  cy = (double)height -1.0 -cy;	// KS correction - casting 
  cmpEyeToImageMat();
}

void PerspectiveProjection::resizeImage(int newW, int newH)
{
	float rx = (float)newW/(float)width;
	float ry = (float)newH/(float)height;
	f *= rx;
	g *= ry;
	s *= rx;
	cx *= rx;
	cy *= ry;

	width = newW;
	height = newH;
	cmpEyeToImageMat();
}

bool PerspectiveProjection::loadOpenCVCalib(const char* filename) {
	cv::FileStorage fs(filename, cv::FileStorage::READ);
    if (!fs.isOpened()) return false;

	cv::Mat K;
	fs["camera_matrix"] >> K;
    int width = fs["image_width"];
    int height = fs["image_height"];
    
    set(width, height, 
        K.at<double>(0,0), K.at<double>(1,1),
        K.at<double>(0,2), K.at<double>(1,2));
	return true;
}


/*! Inverse V axis (projected image upside/down)
 */
void PerspectiveCamera::flip()
{
	PerspectiveProjection::flip();
	cmpWorldToImageMat(); 
}

void PerspectiveCamera::resizeImage(int newW, int newH)
{
	PerspectiveProjection::resizeImage(newW,newH);
	cmpWorldToImageMat(); 
}

bool PerspectiveProjection::getUndistortMap(CvMat *xmap, CvMat *ymap)
{
  if (distortion == 0) return false;

  for (int y=0; y<xmap->height; y++) {
    for (int x=0; x<xmap->width; x++) {
      float image[2], distor[2];

      image[0] = (float)( (x - (cx))/f );
      image[1] = (float)( (y - (cy))/g );

      distor[0] = (float) (image[0] - distortion * image[0] * (image[0]*image[0]+image[1]*image[1]));
      distor[1] = (float) (image[1] - distortion * image[1] * (image[0]*image[0]+image[1]*image[1]));


      cvSetReal2D(xmap, y, x, distor[0]*f + cx);
      cvSetReal2D(ymap, y, x, distor[1]*g + cy);
    }
  }
  return true;
}

PerspectiveCamera::PerspectiveCamera()
{
  clearExternalParams();
  translate(0,0,-f);
}

void PerspectiveCamera::worldToImage(const Vec3 &p, Vec3 &uvw) const
{
  uvw.setMul(worldToImageMat, p);
  uvw.v[0] /= uvw.v[2];
  uvw.v[1] /= uvw.v[2];
}

void PerspectiveCamera::worldToImage(const double p[3], double uvw[3]) const
{
  worldToImage(*((Vec3 *)p), *((Vec3 *)uvw));
}

void PerspectiveCamera::worldToEye(const Vec3 &src, Vec3 &dst) const
{
  dst.setMul(worldToEyeMat, src);
}

void PerspectiveCamera::worldToEye(const double src[3], double dst[3]) const
{
  ((Vec3 *)dst)->setMul(worldToEyeMat, *((Vec3 *)src));
}

void PerspectiveCamera::getGlModelView(double view[4][4]) const
{
  getDirect3DView(view);

  view[0][2] = -view[0][2];
  view[1][2] = -view[1][2];
  view[2][2] = -view[2][2];
  view[3][2] = -view[3][2];
}

void PerspectiveCamera::getDirect3DView(double view[4][4]) const
{
  // Matrix Transposed 
  for(int i=0; i<4; i++)
    for(int j=0; j<3; j++)
      view[i][j] = worldToEyeMat.m[j][i];

  view[0][3] = view[1][3] = view[2][3] = 0;
  view[3][3] = 1;
}



void PerspectiveCamera::setByTarget(const Vec3 pos, const Vec3 target, double roll)
{
  Vec3 x,y,z;

  // TODO: is this correct ?
  Vec3 up(sin(roll), cos(roll), 0);

  z.setSub(target, pos);
  z.normalize();

  x.setCross(z,up);
  y.setCross(x,z);

  for (int i=0; i<3; i++) {
    worldToEyeMat.m[0][i] = x[i];
    worldToEyeMat.m[1][i] = y[i];
    worldToEyeMat.m[2][i] = z[i];
  }

  for (int i=0; i<3; ++i) {
    worldToEyeMat.m[i][3] = -worldToEyeMat.m[i][0]*pos[0]
    - worldToEyeMat.m[i][1]*pos[1]
    - worldToEyeMat.m[i][2]*pos[2];
  }
  cmpWorldToImageMat();
}

static void get3x3MulWithTransposed(const double m[3][4], double mmt[3][3])
{
  double t7 = m[0][0]*m[0][0];
  double t8 = m[0][1]*m[0][1];
  double t9 = m[0][2]*m[0][2];
  double t14 = m[0][0]*m[1][0]+m[0][1]*m[1][1]+m[0][2]*m[1][2];
  double t18 = m[0][0]*m[2][0]+m[0][1]*m[2][1]+m[0][2]*m[2][2];
  double t19 = m[1][0]*m[1][0];
  double t20 = m[1][1]*m[1][1];
  double t21 = m[1][2]*m[1][2];
  double t26 = m[1][0]*m[2][0]+m[1][1]*m[2][1]+m[1][2]*m[2][2];
  double t27 = m[2][0]*m[2][0];
  double t28 = m[2][1]*m[2][1];
  double t29 = m[2][2]*m[2][2];
  mmt[0][0] = t7+t8+t9;
  mmt[0][1] = t14;
  mmt[0][2] = t18;
  mmt[1][0] = t14;
  mmt[1][1] = t19+t20+t21;
  mmt[1][2] = t26;
  mmt[2][0] = t18;
  mmt[2][1] = t26;
  mmt[2][2] = t27+t28+t29;
}

/*!
* This function separate intrinsic and extrinsic parameters from a single
* projection matrix. The resulting rotation matrix (worldToEyeMat) determinant
* is always positive.
*/
void PerspectiveCamera::loadTdir(const double _tdir[3][4], int w, int h)
{
  this->width=w;
  this->height=h;
  double tdir[3][4];

  double ppt[3][3];

  double l = sqrt(_tdir[2][0]*_tdir[2][0]
                + _tdir[2][1]*_tdir[2][1]
                + _tdir[2][2]*_tdir[2][2]);
  for (int i=0; i<3; ++i)
    for (int j=0;j<4; ++j)
      tdir[i][j] = _tdir[i][j]/l;

  get3x3MulWithTransposed(tdir, ppt);

  worldToEyeMat.m[2][0] = tdir[2][0];
  worldToEyeMat.m[2][1] = tdir[2][1];
  worldToEyeMat.m[2][2] = tdir[2][2];
  worldToEyeMat.m[2][3] = tdir[2][3];

  cx = ppt[2][0];
  cy = ppt[2][1];

  // sign is unkown!
  g = sqrt(ppt[1][1]-cy*cy);

  s = (ppt[1][0]-cx*cy)/g;

  // sign is also unknown!
  f = sqrt(ppt[0][0]-cx*cx-s*s);

  worldToEyeMat.m[1][0] = (tdir[1][0] - cy*worldToEyeMat.m[2][0])/g;
  worldToEyeMat.m[0][0] = (tdir[0][0] - s*worldToEyeMat.m[1][0] - cx*worldToEyeMat.m[2][0])/f; 

  worldToEyeMat.m[1][1] = (tdir[1][1] - cy*worldToEyeMat.m[2][1])/g;
  worldToEyeMat.m[0][1] = (tdir[0][1] - s*worldToEyeMat.m[1][1] - cx*worldToEyeMat.m[2][1])/f; 

  worldToEyeMat.m[1][2] = (tdir[1][2] - cy*worldToEyeMat.m[2][2])/g;
  worldToEyeMat.m[0][2] = (tdir[0][2] - s*worldToEyeMat.m[1][2] - cx*worldToEyeMat.m[2][2])/f; 

  worldToEyeMat.m[1][3] = (tdir[1][3] - cy*worldToEyeMat.m[2][3])/g;
  worldToEyeMat.m[0][3] = (tdir[0][3] - s*worldToEyeMat.m[1][3] - cx*worldToEyeMat.m[2][3])/f;

  if (worldToEyeMat.det() < 0) {
    g=-g;
    for (int i=0;i<4;++i) worldToEyeMat.m[1][i] = -worldToEyeMat.m[1][i];
  }

  cmpEyeToImageMat();
  eyeToWorldMat.setInverseByTranspose(worldToEyeMat);
  memcpy(&worldToImageMat, tdir, sizeof(worldToImageMat));
}

/*! Load external and internal parameters from a tdir file. The w and h specify
* the size of the projection rectangle.
*
* \return true on success, false on failure.
*/
bool PerspectiveCamera::loadTdir(const char *tdirFile, int w, int h)
{
  ifstream file;
  file.open(tdirFile);

  if (!file.good()) return false;
  double tdir[3][4];

  for (int j=0; j<3; ++j) 
    for (int i=0; i<4; ++i) 
      file >> tdir[j][i];
  char line[256];
  while (file.getline(line,256)) {
    if (memcmp(line, "distortion:", 11)==0) {
      double d;
      if (sscanf(line+11, "%lf", &d)==1) {
        distortion = d;
        printf("Camera distortion: %f\n", d);
      }
    } else if (memcmp(line, "width:", 6)==0) {
      int nw;
      if (sscanf(line+6, "%d", &nw)==1) {
        w = nw;
      }
    } else if (memcmp(line, "height:", 7)==0) {
      int nh;
      if (sscanf(line+7, "%d", &nh)==1) {
        h = nh;
      }
    } else if (memcmp(line, "near:", 5)==0) {
      double d;
      if (sscanf(line+5, "%lf", &d)==1) {
        nearPlane = d;
        //printf("Near clipping plane: %f\n", d);
      }
    } else if (memcmp(line, "far:", 4)==0) {
      double d;
      if (sscanf(line+4, "%lf", &d)==1) {
        farPlane = d;
        //printf("Near clipping plane: %f\n", d);
      }
    }
  }
  file.close();
  loadTdir(tdir, w,h);

  // look for a .plane file.
  if (strlen(tdirFile) > 5) {
    char fn[512];
    strncpy(fn, tdirFile, 511);
    strcpy(fn + strlen(fn) - strlen(".tdir"), ".plane");
    file.open(fn);

    if (file.good()) {
      double d,n=nearPlane,f=farPlane;
      for (int i=0; i<4; ++i)
        file >> d;
      if (file.good()) file >> n;
      if (file.good()) file >> f;

      file.close();
      if (setPlanes(n,f))
        std::cout << "Reading clipping planes from " << fn 
        << ": near=" << n 
        << " far=" << f << endl;
      else
        std::cerr << "Troubles loading " << fn << "!\n";
    }
  }
  return true;
}

struct GuessMode {
  int w, h;
};
static double diag(double x, double y)
{
  return sqrt(x*x+y*y);
}

static double diagDiff(const GuessMode &m, double dx, double dy)
{
  double mdiag = diag(double(m.w), double(m.h));
  return fabs(double(mdiag - diag(2*dx,2*dy)));
}


/*! Load external and internal parameters from a tdir file, estimating image
 * width and height.
 *
 * THE WIDTH AND HEIGHT ESTIMATION MIGHT BE WRONG!
 *
 *  \return true on success, false on failure.
 */
bool PerspectiveCamera::loadTdir(const char *tdirFile)
{
  if (!loadTdir(tdirFile,0,0)) 
    return false;

  if ((width !=0) && (height !=0)) 
    return true;

  static const GuessMode mode[] = {
    { 640, 480 },
    { 320, 240 },
    { 720, 576 },
    { 360, 288 },
    { 800, 600 },
    { 1024, 768},
    { 512, 384},
    { 1280, 1024},
    { -1, -1}
  };

  int best=0;
  double bestDelta = diagDiff(mode[0], cx, cy);	
  for (int i=1; mode[i].w != -1; ++i) {
    double delta = diagDiff(mode[i], cx, cy);
    if (delta < bestDelta) {
      best = i;
      bestDelta = delta;
    }
  }
  width = mode[best].w;
  height = mode[best].h;

  return true;
}

/*!  
 * Compute external parameters from the view matrix (worldToEyeMat)
 */
void PerspectiveCamera::getExternalParameters(double& tx, double &ty, double &tz, 
                                              double &omega, double &phi, double &kappa)
{
  //                 | r_00  r_01  r_02 t_0 |
  // rotationTrans = | r_10  r_11  r_12 t_1 |
  //                 | r_20  r_21  r_22 t_2 |
  //                 |   0     0     0   1  |

  Mat3x3 matrix;
  Vec3 angles;
  // Vec3 t;

  for(int i = 0; i < 3; i++)
    for(int j = 0; j < 3; j++)
      matrix.m[i][j] = worldToEyeMat.m[i][j];

  // gfla_get_euler_angles_from_rotation(R);
  GetEulerAnglesFromRotation(matrix, angles);

  tx = worldToEyeMat.m[0][3];
  ty = worldToEyeMat.m[1][3];
  tz = worldToEyeMat.m[2][3];
  omega = angles[0]; // rot. autour de OX
  phi   = angles[1]; // rot. autour de OY
  kappa = angles[2]; // rot. autour de OZ
}

/*!
 * Extract Euler angles from rotation and translation matrix.
 * \sa RotationMatrixFromOmegaPhiKappa() for information on order of angles
 */
void PerspectiveCamera::GetEulerAnglesFromRotation(const Mat3x3 &rotationTrans, Vec3 &angles )
{
  // Déjà déclarer dans General.h de la lib stARter
#ifndef M_PI
  const double M_PI = 3.1415926535897932384626433832795;
#endif

  double r_00 = rotationTrans.m[0][0] , r_01 = rotationTrans.m[0][1] , r_02 = rotationTrans.m[0][2];
  double r_10 = rotationTrans.m[1][0] , r_11 = rotationTrans.m[1][1] , r_12 = rotationTrans.m[1][2];
  double r_20 = rotationTrans.m[2][0] , r_21 = rotationTrans.m[2][1] , r_22 = rotationTrans.m[2][2];

  if (fabs(r_22)<1e-6 && fabs(r_21)<1e-6)
  {
    // Cas dégénéré : Quand phi = +/-90 degrés, 
    // ensuite on peut pas séparer omega et kappa
    angles[0] = atan2 (r_12,r_11);
    if(r_20>.0) 
      angles[1] = M_PI/2;
    else 
      angles[1] = - M_PI/2;
    angles[2] = .0;
    // return(0);
  }
  else
  {
    angles[0] = atan2(-r_21,r_22);
    angles[1] = atan2( r_20,sqrt(r_00*r_00+r_10*r_10));
    angles[2] = atan2(-r_10,r_00);
    // return(1);
  }
}

/*
 * Get rotation matrix.
 *
 * Rotation 3x3 Matrix with O=Omega, P=Phi and K=Kappa :
 *
 *              | r_00  r_01  r_02 |   
 *   Rotation = | r_10  r_11  r_12 | = 
 *              | r_20  r_21  r_22 |   
 *  
 * |  cos(K) -sin(K) 0 |   |  cos(P) 0 sin(P) |   | 1   0       0    |
 * |  sin(K)  cos(K) 0 | * |    0    1   0    | * | 0 cos(O) -sin(O) |
 * |   0        0    1 |   | -sin(P) 0 cos(P) |   | 0 sin(O)  cos(O) |
 *
 *     rot. around OZ    *    rot. around OY    *   rot. around de OX
 *
 *  Caution: rotations around the axes do not commutate
 */
void PerspectiveCamera::RotationMatrixFromOmegaPhiKappa(Mat3x3 &rotation, const Vec3 &angles)
{
  double Omega=angles[0],Phi=angles[1],Kappa=angles[2];

  double cos_Omega=cos(Omega),cos_Phi=cos(Phi),cos_Kappa=cos(Kappa);
  double sin_Omega=sin(Omega),sin_Phi=sin(Phi),sin_Kappa=sin(Kappa);

  rotation.m[0][0] = cos_Phi*cos_Kappa;
  rotation.m[0][1] = cos_Omega*sin_Kappa + sin_Omega*sin_Phi*cos_Kappa;
  rotation.m[0][2] = sin_Omega*sin_Kappa - cos_Omega*sin_Phi*cos_Kappa;

  rotation.m[1][0] = -cos_Phi*sin_Kappa;
  rotation.m[1][1] =  cos_Omega*cos_Kappa - sin_Omega*sin_Phi*sin_Kappa;
  rotation.m[1][2] =  sin_Omega*cos_Kappa + cos_Omega*sin_Phi*sin_Kappa;

  rotation.m[2][0] =  sin_Phi;
  rotation.m[2][1] = -sin_Omega*cos_Phi;
  rotation.m[2][2] =  cos_Omega*cos_Phi;
}


void PerspectiveCamera::clearExternalParams()
{
  worldToEyeMat.setIdentity();
  cmpWorldToImageMat();
}

void PerspectiveCamera::cmpWorldToImageMat()
{
  worldToImageMat.setMul(eyeToImageMat, worldToEyeMat);
  eyeToWorldMat.setInverseByTranspose(worldToEyeMat);
}

void PerspectiveCamera::eyeToWorld(const Vec3 &uvw, Vec3 &w) const
{
  w.setMul(eyeToWorldMat, uvw);
}

void PerspectiveCamera::imageToWorld(double u, double v, Vec3 &w, double depth) const
{
  Vec3 uvw;
  imageToEye(u,v, uvw.v, depth);
  eyeToWorld(uvw, w);
}

void PerspectiveCamera::translate(double dx, double dy, double dz) 
{
  worldToEyeMat.m[0][3] -= dx;
  worldToEyeMat.m[1][3] -= dy;
  worldToEyeMat.m[2][3] -= dz;
  cmpWorldToImageMat();
}

void PerspectiveCamera::setWorldToEyeMat(const Mat3x4 &m)
{
  worldToEyeMat = m;
  cmpWorldToImageMat();
}

bool PerspectiveCamera::saveTdir(const char *file)
{
  ofstream f(file);

  if (!f.good()) return false;

  for (int l=0; l<3; l++) {
    for (int c=0; c<4; c++) {
      f << worldToImageMat.m[l][c] << " ";
    }
    f << "\n";
  }
  f << "width: " << width << endl;
  f << "height: " << height << endl;
  f << "distortion: " << distortion << endl;
  f << "near: " << nearPlane << endl;
  f << "far: " << farPlane << endl;
  return true;
}

ostream& operator << (ostream& os, const PerspectiveProjection &cam)
{
  os << "fx=" << cam.f
     << ", fy="<< cam.g
     << ", cx="<< cam.cx
     << ", cy="<< cam.cy
     << ", s="<< cam.s
     << ", near="<< cam.nearPlane
     << ", far="<< cam.farPlane
     << ", width="<<cam.width
     << ", height="<<cam.height;
  return os;
}

ostream& operator << (ostream& os, const PerspectiveCamera &cam) 
{
  os << ((PerspectiveProjection) cam) << endl;
  os << cam.getWorldToEyeMat();
  return os;
}


static void inverse(const double m[3][3], double dst[3][3])
{
    double t4 = m[0][0]*m[1][1];
    double t6 = m[0][0]*m[1][2];
    double t8 = m[0][1]*m[1][0];
    double t10 = m[0][2]*m[1][0];
    double t12 = m[0][1]*m[2][0];
    double t14 = m[0][2]*m[2][0];
    double t16 = (t4*m[2][2]-t6*m[2][1]-t8*m[2][2]+t10*m[2][1]+t12*m[1][2]-t14*m
	    [1][1]);
    double t17 = 1/t16;
    dst[0][0] = (m[1][1]*m[2][2]-m[1][2]*m[2][1])*t17;
    dst[0][1] = -(m[0][1]*m[2][2]-m[0][2]*m[2][1])*t17;
    dst[0][2] = -(-m[0][1]*m[1][2]+m[0][2]*m[1][1])*t17;
    dst[1][0] = -(m[1][0]*m[2][2]-m[1][2]*m[2][0])*t17;
    dst[1][1] = (m[0][0]*m[2][2]-t14)*t17;
    dst[1][2] = -(t6-t10)*t17;
    dst[2][0] = -(-m[1][0]*m[2][1]+m[1][1]*m[2][0])*t17;
    dst[2][1] = -(m[0][0]*m[2][1]-t12)*t17;
    dst[2][2] = (t4-t8)*t17;
}

/*
  Projection:
   u = K * Rt * [x,y,0,1]' = H * [x,y,1]'
   inv(K) * H = lambda * [R1 R2 T]
*/

void PerspectiveCamera::setPoseFromHomography(const double H_array[3][3]) {
    const cv::Mat H = cv::Mat(3, 3, CV_64FC1, const_cast<double *>(&H_array[0][0]));
    const cv::Mat K = cv::Mat(3, 3, CV_64FC1, eyeToImageMat.m);
    const cv::Mat KinvH = K.inv() * H;
    const double n0 = norm(KinvH.col(0));
    const double n1 = norm(KinvH.col(1));

    // cout << "n0: " << n0 << ", n1: " << n1 << endl;

    Mat3x4 wte;
    cv::Mat result = cv::Mat(3, 4, CV_64FC1, wte.m);
    cv::Mat col0 = result.col(0);
    cv::Mat col1 = result.col(1);
    cv::Mat col2 = result.col(2);

    col0 = KinvH.col(0) / n0;
    col1 = KinvH.col(1) / n1;
    col0.cross(col1).copyTo(col2);
    result.col(3) = KinvH.col(2) / ((n0 + n1) * .5);

    setWorldToEyeMat(wte);
}

