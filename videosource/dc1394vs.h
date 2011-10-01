//! \ingroup videosource
/*!@{*/
#ifndef __DC1394VS_H
#define __DC1394VS_H

#include <cv.h>
#include "iniparser.h"
#include "videosource.h"
#include <dc1394/dc1394.h>

//! Linux dc1394 video source
class DC1394 : public VideoSource {
public:

	struct Params {
		bool debug;
		bool gray;
		int width;
		int height;
                int framerate;
                int bus_speed;
	};

	virtual bool initialize();
	virtual bool getFrame(IplImage *dst);
	virtual void getSize(int &width, int &height);
	DC1394(Params &p);
	virtual ~DC1394();
	virtual void start();
	virtual void stop();
	virtual bool isPlaying() { return playing; };
	virtual int getChannels();
	virtual const char *getStreamName() const { return "DC1394 live stream"; }
	virtual const char *getStreamType() const { return "DC1394"; }
        virtual void *getInternalPointer() { return camera; }
private:
	bool setupCapture();
        bool init_video_mode();

	bool playing;
	IplImage *image;
	Params params;

	dc1394_t *d;
	dc1394camera_t *camera;
	dc1394video_frame_t *frame;
	unsigned int width, height;
	int channels;

};

class DC1394Factory : public ParticularVSFactory {
public:
	DC1394Factory() { name="DC1394"; };
	virtual void registerParameters(ParamSection *sec);
	virtual VideoSource *construct();
private:
	bool use;
	DC1394::Params params;
};

#endif
/*!@}*/
