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
/****************************************************************************
** $Id: glbox.cpp,v 1.8 2008/10/30 15:11:44 jpilet Exp $
**
** Copyright (C) 1992-2000 Trolltech AS.  All rights reserved.
**
** This file is part of an example program for Qt.  This example
** program may be used, distributed and modified without limitation.
**
*****************************************************************************/

#include <iostream>
#include <math.h>
#include <highgui.h>

using namespace std;

#include <qpixmap.h>
#if QT_VERSION >= 0x040000
#include <QMouseEvent>
#endif
#include "glbox.h"
#include <stdlib.h>

#if defined(Q_CC_MSVC)
#pragma warning(disable:4305) // init: truncation from const double to float
#endif

/*!
  Create a GLBox widget
  */

GLBox::GLBox(QWidget* parent, const char* , const QGLWidget* shareWidget)
: QGLWidget( parent, shareWidget )
{
	init();
}

GLBox::GLBox(const QGLFormat &format, QWidget *parent, const char *, const QGLWidget* shareWidget)
: QGLWidget( format, parent, shareWidget )
{
	init();
}

void GLBox::init() 
{
	textureWidth = 0;
	textureHeight = 0;
	image=0;
	dx=dy=0;
	sx=sy=1;
	reloadImage = true;
	allowCache = false;
	yAxisUp = false;
	smooth=false;
	setupTransform();
}

/*!
  Release allocated resources
  */

GLBox::~GLBox()
{
	makeCurrent();
	if (texture!=0)
		glDeleteTextures( 1, &texture );
}


/*!
  Paint the box. The actual openGL commands for drawing the box are
  performed here.
  */

void GLBox::paintGL()
{

	glClearColor(0,0,0,0);
	glClear( GL_COLOR_BUFFER_BIT );

	if (image ==0) return;


	float w = float(image->width);
	float h = float(image->height);
	float u = w/float(textureWidth);
	float v = h/float(textureHeight);
	float vUp = 0;

	if (image->origin) {
		// upside down
		vUp = v;
		v = 0;
	}

	float imUp = 0;

	if (yAxisUp) {
		imUp = h;
		h = 0;
	}

	setImageSpace();

	glDisable(GL_BLEND);
	loadTexture();
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (smooth ? GL_LINEAR : GL_NEAREST));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, /*(smooth ?*/ GL_LINEAR /*: GL_NEAREST*/);

	glBegin(GL_QUADS);

	glTexCoord2f(u,v);
	glVertex2f(w,h);

	//              glTexCoord2f(0,1);
	glTexCoord2f(u,vUp);
	glVertex2f(w,imUp);

	//              glTexCoord2f(1,1);
	glTexCoord2f(0,vUp);
	glVertex2f(0,imUp);

	//              glTexCoord2f(1,0);
	glTexCoord2f(0,v);
	glVertex2f(0,h);

	glEnd();

	glDisable(GL_TEXTURE_2D);

}


/*!
  Set up the OpenGL rendering state, and define display list
  */

void GLBox::initializeGL()
{
	reloadImage = true;

	glClearColor( 1,1,1,1 ); 		// Let OpenGL clear to black

	//texture = genGlTexture(textureSize, textureSize, smooth);
}

GLuint GLBox::genGlTexture(int texWidth, int texHeight, bool smooth) {
	GLuint tex;


	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glGenTextures(1,&tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (smooth ? GL_LINEAR : GL_NEAREST));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (smooth ? GL_LINEAR : GL_NEAREST));

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texWidth,texHeight, 0, GL_RGBA,
			GL_UNSIGNED_BYTE, 0);

	return tex;
}

/*!
  Set up the OpenGL view port, matrix mode, etc.
  */

void GLBox::resizeGL( int w, int h )
{
	glViewport( 0, 0, (GLint)w, (GLint)h );
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	glMatrixMode( GL_MODELVIEW );
}

void GLBox::setImage( IplImage *image ) {

	this->image = image;
	reloadImage = true;
}
	
void GLBox::screenToImageCoord(int x, int y, float *imx, float *imy) const
{
	float w = (float)width();
	float h = (float)height();

	if (imx) *imx=0;
	if (imy) *imy=0;
	if (!image) return;
	
	float fx = (float(x)*(float)image->width)/w;
	float fy = (float(y)*(float)image->height)/h;

	if (imx) *imx = invAffineTransf[0][0]*fx + invAffineTransf[0][1]*fy + invAffineTransf[0][2];
	if (imy) *imy = invAffineTransf[1][0]*fx + invAffineTransf[1][1]*fy + invAffineTransf[1][2];
}

#ifdef WIN32
int lrint(float f) {
	return (int) floor(f+0.5);
}
#endif

void GLBox::imageToScreenCoord(float x, float y, int *scrx, int *scry) const
{
	
	float w = (float)width();
	float h = (float)height();

	float imw=640;
	float imh=480;

	if (image) {
		imw = image->width;
		imh = image->height;
	}
	
	if (scrx) *scrx = lrint((affineTransf[0][0]*x + affineTransf[0][1]*y + affineTransf[0][2])*w/imw);
	if (scry) *scry = lrint((affineTransf[1][0]*x + affineTransf[1][1]*y + affineTransf[1][2])*h/imh);
}

void GLBox::setupTransform() {

	float h,f;
       
	if (image) {
		h = (float)image->height;
		f = yAxisUp ? -1 : 1;
	} else {
		h = 0;
		f = 1;
	}

	affineTransf[0][0] = sx;
	affineTransf[1][0] = 0;
	//affineTransf[2][0] = 0;

	affineTransf[0][1] = 0;
	affineTransf[1][1] = sy*f;
	//affineTransf[2][1] = 0;

	affineTransf[0][2] = dx*affineTransf[0][0];
	affineTransf[1][2] = dy*affineTransf[1][1] + (sy*(1-f)*h)/2;
	//affineTransf[2][2] = 1;

	invertAffine();
}
	
// libvision support
#ifdef WITH_LIBVISION

void iplShareRaster(VL::RasterImage *ri, IplImage *ipl)
{
	memset(ipl, 0, sizeof(IplImage));
	switch (ri->bytes) {
		case 1: ipl->depth = IPL_DEPTH_8U; 
			ipl->nChannels = 1;
			memcpy(ipl->colorModel, "GRAY", 4);
			memcpy(ipl->channelSeq, "GRAY", 4);
			break;
		case 3: ipl->depth = IPL_DEPTH_8U; 
			ipl->nChannels = 3;
			strcpy(ipl->colorModel, "RGB");
			strcpy(ipl->channelSeq, "RGB"); // or is it BGR ?
			break;
		case 4: ipl->depth = IPL_DEPTH_32F; 
			ipl->nChannels = 1;
			break;
		default:
			std::cerr <<  "iplShareRaster(): unknown format !\n";
			abort();
	}
	ipl->width = ri->xdim;
	ipl->height = ri->ydim;
	ipl->imageData = (char *)ri->array;
	ipl->nSize = sizeof(IplImage);
	ipl->widthStep = ipl->width * ri->bytes;
	ipl->imageSize = ipl->widthStep * ipl->height;
	ipl->align = 1;
}

void GLBox::setImage(VL::RasterImage *im)
{
	iplShareRaster(im, &iplIm);
	setImage(&iplIm);
}

#endif

void GLBox::setImageSpace()
{
	GLfloat m[16];
	double w=640,h=480;

	if (image) {
		w = image->width;
		h = image->height;
	}

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, w, h, 0, -10, 10);

	m[0] = affineTransf[0][0];
	m[1] = affineTransf[1][0];
	m[2] = 0;
	m[3] = 0;

	m[4] = affineTransf[0][1];
	m[5] = affineTransf[1][1];
	m[6] = 0;
	m[7] = 0;

	m[8] = 0;
	m[9] = 0;
	m[10] = 1;
	m[11] = 0;

	m[12] = affineTransf[0][2] /*+ .45f*/;
	m[13] = affineTransf[1][2] /*+ .45f*/;
	m[14] = 0;
	m[15] = 1;

	glMultMatrixf(m);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void GLBox::saveContext() {
	savedTexture = texture;
}

void GLBox::restoreContext() {
	texture = savedTexture;
}

bool GLBox::renderAndSave(const char *filename, int , int )
{
	show();
	raise();
	//setActiveWindow();

	makeCurrent();
	paintGL();

	IplImage *buffer = cvCreateImage(cvSize(width(), height()), IPL_DEPTH_8U, 3);
	glReadPixels(0, 0, width(), height(), GL_BGR_EXT, GL_UNSIGNED_BYTE, buffer->imageData);

	buffer->origin = 1;
	cvSaveImage(filename, buffer);
	cvReleaseImage(&buffer);
	return true;

}

void GLBox::loadTexture()
{
	glBindTexture(GL_TEXTURE_2D, texture);
	if (reloadImage) {
		loadGlTexture(image);
		if (allowCache) reloadImage = false;
	}
}

void GLBox::loadGlTexture(IplImage *im) {

	if (!im) return;

	if (texture && (textureWidth < im->width || textureHeight < im->height)) {
		glDeleteTextures( 1, &texture );
		texture = 0;
	}
	if (texture==0) {
		texture = genGlTexture(im->width, im->height, smooth);
		textureWidth = im->width;
		textureHeight = im->height;
	}

	glBindTexture(GL_TEXTURE_2D, texture);

	GLenum format;
	GLenum type;
	switch (im->depth) {
		case IPL_DEPTH_8U: type = GL_UNSIGNED_BYTE; break;
		case IPL_DEPTH_8S: type = GL_BYTE; break;
		case IPL_DEPTH_16S: type = GL_SHORT; break;
		case IPL_DEPTH_32F: type = GL_FLOAT; break;
		default:
				    cerr << "GLBox::paintGL(): unsupported pixel type.\n";
				    return;
	}
	switch (im->nChannels) {
		case 1: format = GL_LUMINANCE; break;
		case 3: format = (im->channelSeq[0] == 'B') ? GL_BGR_EXT : GL_RGB; break;
		case 4: format = GL_RGBA; break;
		default:
			cerr << "GLBox::paintGL(): unsupported number of channels.\n";
			return;
	}

	// pixel storage mode
	glPixelStorei(GL_UNPACK_ALIGNMENT, im->align);

	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, im->width, im->height,
			format, type, im->imageData);
}

void GLBox::mousePressEvent(QMouseEvent *e)
{
	if (e->button() == Qt::LeftButton) {
		lastMousePos = e->pos();
	}
}

void GLBox::mouseMoveEvent ( QMouseEvent * e )
{
	float x1, y1, x2, y2;
	screenToImageCoord(lastMousePos.x(), lastMousePos.y(), &x1, &y1);
	screenToImageCoord(e->pos().x(), e->pos().y(), &x2, &y2);

	lastMousePos = e->pos();
	dx += (x2 - x1);
	dy += (y2 - y1);

	setupTransform();
	update();
}

void GLBox::wheelEvent( QWheelEvent *e )
{
	e->accept();
	
	int delta = e->delta();
	float s= delta / 110.0f;

	if (delta < 0)
		s = 1/(-s);
		
	sx *= s;
	sy *= s;
	setupTransform();
	update();
}

void GLBox::invertAffine() {
	float t4 = 1/(-affineTransf[1][0]*affineTransf[0][1]+affineTransf[0][0]*affineTransf[1][1]);
	invAffineTransf[0][0] = affineTransf[1][1]*t4;
	invAffineTransf[0][1] = -affineTransf[0][1]*t4;
	invAffineTransf[0][2] = (affineTransf[0][1]*affineTransf[1][2]-affineTransf[0][2]*affineTransf[1][1])*t4;
	invAffineTransf[1][0] = -affineTransf[1][0]*t4;
	invAffineTransf[1][1] = affineTransf[0][0]*t4;
	invAffineTransf[1][2] = -(affineTransf[0][0]*affineTransf[1][2]-affineTransf[0][2]*affineTransf[1][0])*t4;
}

