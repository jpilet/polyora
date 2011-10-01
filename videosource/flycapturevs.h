//! \ingroup videosource
/*!@{*/
#ifndef __FLYCAPTUREVS_H
#define __FLYCAPTUREVS_H

#include "videosource.h"
#include <pgrflycapture.h>

//! Basic support for Pointgrey's flycapture API. Possibly outdated.
class FlyCaptureVideoSource : public VideoSource
{
public:

	FlyCaptureVideoSource();

	virtual bool initialize();
	virtual bool getFrame(IplImage *dst);
	void getSize(int &width, int &height);
	virtual ~FlyCaptureVideoSource();
	virtual void start();
	virtual void stop();
	virtual bool isPlaying() { return playing; };
	virtual const char *getStreamName() const { return "FlyCapture live stream"; }
	virtual const char *getStreamType() const { return "FlyCapture"; }

private:
	FlyCaptureError error;
	FlyCaptureContext context;
	FlyCaptureVideoMode videoMode;
	FlyCaptureImage pImage;
	IplImage *tempImageColor;
	IplImage *tempImageGray ;
	IplImage *tempImageColorResized ;
	bool playing;
	bool imagesExist;
};

class FlyCaptureFactory : public ParticularVSFactory {
public:
	FlyCaptureFactory() { name="FlyCapture"; };
	virtual void registerParameters(ParamSection *sec);
	virtual VideoSource *construct();
private:
	bool use;
};
#endif
/*!@}*/
