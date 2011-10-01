//! \ingroup videosource
/*!@{*/
#ifndef __OPENCVVIDEOSOURCE_H
#define __OPENCVVIDEOSOURCE_H

#include "videosource.h"

#ifndef MACOS
#include <highgui.h>
#endif

/*!
 * proxy for OpenCV's cvCapture functions.
 */
class OpenCVVideoSource : public VideoSource {
public:

	OpenCVVideoSource(char *source, int w, int h);
	virtual bool initialize();
	virtual bool getFrame(IplImage *dst);
	void getSize(int &width, int &height);
	virtual ~OpenCVVideoSource();
	virtual void start();
	virtual void stop();
	virtual bool isPlaying() { return playing; };
	virtual const char *getStreamName() const { return source; }
	virtual const char *getStreamType() const { return "OpenCVVideoSource"; }
	virtual int getId();

private:

	CvCapture *capture;
	char *source;
	int frameCnt;
	bool playing;
	IplImage *frame;
	int width,height;
	bool movie;
};

class OpenCVFactory : public ParticularVSFactory {
public:
	OpenCVFactory() { name="OpenCV"; source=0; };
	virtual void registerParameters(ParamSection *sec);
	virtual VideoSource *construct();
private:
	char *source;
	bool use;
	int width,height;
};
#endif
/*!@}*/
