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
#include <iostream>
#include <QtOpenGL>
#include "ipltexture.h"

IplTexture::IplTexture(const IplTexture &a)
{
	downsampled=0;
	textureGenerated=false;
	*this = a;
}
const IplTexture &IplTexture::operator = (const IplTexture &a)
{
	this->~IplTexture();
	if (a.im == &a.header) {
		header = a.header;
		im = &header;
	} else {
		im = a.im;
	}
	allowCache = a.allowCache;
	reload = true;
	smooth = a.smooth;
	textureGenerated=false;
	refcnt = 1;
	isAlpha = a.isAlpha;
	return *this;
}

IplTexture::~IplTexture() 
{
	if (downsampled) cvReleaseImage(&downsampled);

	if (textureGenerated) {
		glDeleteTextures(1,(GLuint*) &texture);
	}
}

void IplTexture::genTexture()
{
#ifdef HAVE_GL
	if (im==0) return;

	if (!textureGenerated) {
		glGenTextures(1,(GLuint*) &texture);
		textureGenerated = true;
	}
	glBindTexture(GL_TEXTURE_2D, texture);

	for (texWidth=1; texWidth < im->width; texWidth <<=1) {}
	for (texHeight=1; texHeight < im->height; texHeight <<=1) {}

	if (texWidth > 1024) texWidth = 1024;
	if (texHeight > 1024) texHeight = 1024;
	//std::cout << "IplTexture *: "<< this << ": generating a " << texWidth << "x" << texHeight << " texture for a "
	//	<< im->width << "x" << im->height << " image.\n";

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (smooth ? GL_LINEAR : GL_NEAREST));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (smooth ? GL_LINEAR : GL_NEAREST));

	int sz = texWidth*texHeight*4;
	char *buffer = (char *) malloc(sz);
	memset(buffer, 0, sz);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texWidth,texHeight, 0, GL_RGBA,
			GL_UNSIGNED_BYTE, buffer);
	free(buffer);
#endif
}

void IplTexture::loadTexture()
{
#ifdef HAVE_GL
	if (im==0) return;
	if (!textureGenerated) genTexture();

	IplImage *im = (downsampled ? downsampled : this->im);

	if ((im->width > texWidth) || (im->height > texHeight)) {
		if (downsampled) cvReleaseImage(&downsampled);
		downsampled = cvCreateImage(cvSize(texWidth, texHeight), this->im->depth, this->im->nChannels);
		im = downsampled;
	}

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texture);

	if (allowCache && !reload) return;
	reload = false;

	if (downsampled)
		cvResize(this->im, downsampled, CV_INTER_AREA);

	GLenum format;
	GLenum type;
	switch (im->depth) {
		case IPL_DEPTH_8U: type = GL_UNSIGNED_BYTE; break;
		case IPL_DEPTH_8S: type = GL_BYTE; break;
		case IPL_DEPTH_16S: type = GL_SHORT; break;
		case IPL_DEPTH_32F: type = GL_FLOAT; break;
		default:
				    std::cerr << "IplTexture::loadTexture(): unsupported pixel type.\n";
				    return;
	}
	switch (im->nChannels) {
		case 1: format = (isAlpha ? GL_ALPHA : GL_LUMINANCE); break;
		case 3: format = (im->channelSeq[0] == 'B') ? GL_BGR_EXT : GL_RGB; break;
		case 4: format = GL_RGBA; break;
		default:
			std::cerr << "IplTexture::loadTexture(): unsupported number of channels.\n";
			return;
	}

	// pixel storage mode
	glPixelStorei(GL_UNPACK_ALIGNMENT, im->align);

	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, im->width, im->height,
			format, type, im->imageData);

	uScale = (double(im->width)/double(this->im->width))/double(texWidth);
	vScale = (double(im->height)/double(this->im->height))/double(texHeight);
	vOrigin = 0;

	if (im->origin) {
		vScale = -vScale;
		vOrigin = double(im->height)/double(texHeight);
	}
#endif
}

void IplTexture::disableTexture()
{
#ifdef HAVE_GL
	glDisable(GL_TEXTURE_2D);
#endif
}

void IplTexture::unref()
{
	refcnt--;
	if (refcnt <= 0)
		delete this;
}

void IplTexture::setImage(IplImage *image)
{
	im = image;
	if (downsampled) cvReleaseImage(&downsampled);
	update();
}

void IplTexture::setImage(CvMat *mat)
{
	setImage(cvGetImage(mat, &header));
}

void IplTexture::regen()
{
       	update(); 
	textureGenerated = false; 
}

void IplTexture::drawQuad(float r) {
	drawQuad(-r,-r,2*r,2*r);
}

//!\brief  Draw a frame contained in an IplTexture object on an OpenGL viewport.
void IplTexture::drawQuad(float x, float y, float _width, float _height)
{
	if (!this || !getIm()) return;

	IplImage *im = getIm();
	int w = im->width-1;
	int h = im->height-1;

	float width = (_width==0 ? im->width : _width);
	float height = (_height==0 ? im->height : _height);


	loadTexture();

	glBegin(GL_QUADS);

	glTexCoord2f(u(0), v(0));
	glVertex2f(x, y);

	glTexCoord2f(u(w), v(0));
	glVertex2f(x+width, y);

	glTexCoord2f(u(w), v(h));
	glVertex2f(x+width, y+height);

	glTexCoord2f(u(0), v(h));
	glVertex2f(x, y+height);
	glEnd();

	disableTexture();
}

#ifdef ___MACOS
#include <QuickTime/QuickTime.h>
#include <QuickTime/Movies.h>
#include <QuickTime/QuickTimeComponents.h>

bool LoadTexture(IplTexture *tex, const char *filename, const char *subdir)
{
    ComponentInstance fileImporter;
    OSErr error;
    ImageDescriptionHandle imageInfo;
    ComponentResult err;


	CFStringRef name = CFStringCreateWithCStringNoCopy(NULL, filename, kCFStringEncodingASCII, NULL);
	CFStringRef _subdir = CFStringCreateWithCStringNoCopy(NULL, subdir, kCFStringEncodingASCII, NULL);

	CFURLRef texture_url = CFBundleCopyResourceURL(
			CFBundleGetMainBundle(),
			name,
			NULL,
			_subdir);
    
    // Create the data reference so we can get the file's graphics importer.
    Handle dataRef;
    OSType dataRefType;
    
    dataRef = NewHandle(sizeof(AliasHandle));
    
    // The second parameter to QTNewDataReferenceFromCFURL is flags.
    // It should be set to 0.
    error = QTNewDataReferenceFromCFURL(texture_url, 0, &dataRef, &dataRefType);
    if(error != noErr) {
        //DisposeHandle(dataRef);
        //CFRelease(texture_url);
        return false;
    }
    
    // Get the importer for the file so we can read the information.
    error = GetGraphicsImporterForDataRef(dataRef, dataRefType, &fileImporter);
    
    // Retrieve information about the image
    imageInfo = (ImageDescriptionHandle)NewHandle(sizeof(ImageDescription));
    err = GraphicsImportGetImageDescription(fileImporter, &imageInfo);
    unsigned width = ((**imageInfo).width);
    unsigned height = ((**imageInfo).height);
  
		IplImage *im = tex->getIm();
	if (im && (im->width != width || im->height!=height))
		cvReleaseImage(&im);
	if (im==0) {
		im = cvCreateImage(cvSize(width,height), IPL_DEPTH_8U, 4);
		//memset(im->imageData, 0x8f, 30*im->widthStep );
		//memset(im->imageData+ 50*im->widthStep, 0xff, 30*im->widthStep);
		tex->setImage(im);
		//return true;
	}
	tex->update();
	
	void *imageData = im->imageData;
  
    // Get the boundary rectangle of the image
    Rect imageRect;
    err = GraphicsImportGetNaturalBounds(fileImporter, &imageRect);
    
    // Create an offscreen buffer to hold the image.
    // Apparently QuickTime requires a GWorld to
    // decompress a texture file.
    long bytesPerRow = im->widthStep;
	//static
	GWorldPtr offscreenBuffer=0;
	
	if (offscreenBuffer==0) {
		error = QTNewGWorldFromPtr(&offscreenBuffer, k32RGBAPixelFormat, &imageRect,
								NULL, NULL, kNativeEndianPixMap, imageData, bytesPerRow);
		assert(error == noErr);
    }
	
    // Draw the image into the offscreen buffer
    err = GraphicsImportSetGWorld(fileImporter, offscreenBuffer, NULL);
	assert(err == noErr);
    err = GraphicsImportDraw(fileImporter);
    assert(err == noErr);
    
    // Cleanup
    error = CloseComponent(fileImporter);
    DisposeHandle((Handle)imageInfo);
    DisposeHandle(dataRef);
	DisposeGWorld(offscreenBuffer);
	return true;
}
#endif


