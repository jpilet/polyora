//! \ingroup videosource
/*!@{*/
#ifndef __BMPVIDEOSOURCE_H
#define __BMPVIDEOSOURCE_H

#include "videosource.h"

/*!
 * Load a sequence of images using a "printf" pattern.
 * cvLoadImage is used, so many file format are supported.
 */
class BmpVideoSource : public VideoSource {
public:

	BmpVideoSource(char *genericFilename, int first, int last);
	virtual bool initialize();
	virtual bool getFrame(IplImage *dst);
	void getSize(int &width, int &height);
	virtual ~BmpVideoSource();
	virtual void start();
	virtual void stop();
	virtual bool isPlaying() { return playing; };
	virtual const char *getStreamName() const { return genericFilename; }
	virtual const char *getStreamType() const { return "BmpVideoSource"; }
	virtual int getId();

private:

	char *genericFilename;
	int frameCnt;
	bool playing;
	int firstImageIndex;
	int lastImageIndex;
	int width,height;
};

class BmpFactory : public ParticularVSFactory {
public:
	BmpFactory() { name="Bmp"; filename=0; };
	virtual void registerParameters(ParamSection *sec);
	virtual VideoSource *construct();
private:
	char *filename;
	bool use;
	int first, last;
};
#endif
/*!@}*/
