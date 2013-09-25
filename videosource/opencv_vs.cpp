
#include <stdio.h>
#include <iostream>

#include "opencv_vs.h"
#include "iniparser.h"

using namespace std;

void OpenCVFactory::registerParameters(ParamSection *sec) {
	sec->addStringParam("opencv.source", &source, "0");
	sec->addBoolParam("opencv.use", &use, true);
	sec->addIntParam("opencv.width", &width, -1);
	sec->addIntParam("opencv.height", &height, -1);
};

VideoSource *OpenCVFactory::construct() {
	if (use) {
		OpenCVVideoSource *vs = new OpenCVVideoSource(source, width, height);
		if (vs->initialize()) return vs;
		delete vs;
	}
	return 0;
}

OpenCVVideoSource::OpenCVVideoSource(char *source, int w, int h)
	: source(source), width(w), height(h)
{	
	playing=true;
	frameCnt = 0;
	frame = 0;
	movie = false;
}

bool OpenCVVideoSource::initialize() 
{
	if (strlen(source)==0 || (strlen(source)==1 && isdigit(source[0])))
		capture = cvCaptureFromCAM(strlen(source)>0?(source[0]-'0'):0);
	else {
		capture = cvCaptureFromAVI(source);
		movie = true;
	}
	
	if(width>0 && height>0) {
		cvSetCaptureProperty(capture,CV_CAP_PROP_FRAME_WIDTH, width);
		cvSetCaptureProperty(capture,CV_CAP_PROP_FRAME_HEIGHT, height);
	}

	return capture!=0;
}

bool OpenCVVideoSource::getFrame(IplImage *dst) 
{

	if (!frame || playing) {
		frame = cvQueryFrame( capture );
		frameCnt++;
	}

	if (!frame) return false;

	if (frame->depth == dst->depth) {
		if (frame->nChannels == 3 && dst->nChannels==1) {
			cvCvtColor(frame,dst, CV_BGR2GRAY);
		} else if (frame->width == dst->width && frame->height==dst->height)
			cvCopy(frame,dst);
		else
			cvResize(frame,dst);
	} else {
		if (frame->width == dst->width && frame->height==dst->height)
			cvConvertScale(frame,dst);
		else {
			IplImage *tmp = cvCreateImage(cvGetSize(frame), dst->depth, dst->nChannels);
			cvConvertScale(frame,tmp);
			cvResize(tmp,dst);
			cvReleaseImage(&tmp);
		}
	}
	return true;
}

int OpenCVVideoSource::getId() 
{
	if (movie) {
		double pos = cvGetCaptureProperty(capture, CV_CAP_PROP_POS_MSEC);
		double fps = cvGetCaptureProperty(capture, CV_CAP_PROP_FPS);
		return pos;
		//return (int) pos;
	} else 
		return frameCnt;
}

OpenCVVideoSource::~OpenCVVideoSource() {
}

void OpenCVVideoSource::start() {
	playing = true;
}

void OpenCVVideoSource::stop() {
	playing = false;
}


void OpenCVVideoSource::getSize(int &width, int &height)
{
    for (int i = 0; frame == 0 && i < 10; ++i) {
        frame = cvQueryFrame( capture );
    }
    if (!frame) {
        fprintf(stderr, "OpenCVVideoSource: failed to get video size.\n");
        width = 640;
        height = 480;
    } else {
        width = frame->width;
        height = frame->height;
    }
}
