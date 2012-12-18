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
/*
 * Julien Pilet, 2005-2008
 */
#ifndef _IPLTEXTURE_H
#define _IPLTEXTURE_H

#include <opencv/cv.h>

#define HAVE_GL

#ifdef HAVE_CONFIG_H
#include <config.h>
#else
#ifdef WIN32
	// Comment this out if you do not have GLEW installed.
// #define HAVE_GLEW
#endif
#endif

#ifdef HAVE_GLEW
#include <GL/glew.h>
#ifndef HAVE_GL
#define HAVE_GL
#endif
#else
#ifdef WIN32
#include <windows.h>
#include <GL/gl.h>

#ifndef GL_BGR_EXT
#define GL_BGR_EXT 0x80E0
#endif
#define HAVE_GL
#endif

#ifdef HAVE_GL
#ifdef MACOS
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#endif
#endif


/*!
 *
 * Represent a texture in video memory, with optional caching.
 *
 * Honours many IplImage fields like nChannels, depth, align, and origin.
 */
class IplTexture {

public:
	IplTexture(IplImage *image=0, bool cache=true, bool smooth=true) 
		: im(image), downsampled(0), allowCache(cache), reload(true),  
		smooth(smooth), textureGenerated(false), refcnt(1), isAlpha(false) {}
	IplTexture(const IplTexture &a);
	const IplTexture &operator = (const IplTexture &a);
	
	virtual ~IplTexture();
		
	//! Only call genTexture from a valid OpenGL context !
	void genTexture();
	void loadTexture();
	void disableTexture();
	void update() { reload=true; }
	void setImage(IplImage *image);
	void setImage(CvMat *mat);
	IplImage *getImage() { return im; }
	IplImage *getIm() { return im; }
	const IplImage *getIm() const { return im; }
	void freeImage() { if (this && im) { cvReleaseImage(&im); } }

	//! Get the U texel coordinates from pixel coordinate x.
	double u(double x) { return x*uScale; }

	//! Get the V texel coordinates from pixel coordinate y (axis pointing down).
	double v(double y) { return y*vScale + vOrigin; }

	//! force texture regeneration.
	void regen(); 

	//! Add a reference to the reference counter.
	void addRef() { refcnt++; }

	/*! Removes a reference to the reference counter, and delete the
	 *  texture if it reaches 0.
	 */
	void unref();
	
	void clearWithoutDelete() { im = downsampled = 0; }

	void drawQuad(float r=1);
	void drawQuad(float x, float y, float w=0, float h=0);

private:
	IplImage *im;
	IplImage *downsampled;
	IplImage header;

	bool allowCache;
	bool reload;
	bool smooth;
	bool textureGenerated;
	int refcnt;
public:
	bool isAlpha;
private:
	unsigned int texture;
	double uScale, vScale, vOrigin;
	int texWidth, texHeight;
};

bool LoadTexture(IplTexture *tex, const char *filename, const char *subdir);

#endif
