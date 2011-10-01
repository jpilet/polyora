
#include "flycapturevs.h"
#include "iniparser.h"
#include <iostream>

#define DRAGONFLY 0
#define FLEA 1

void FlyCaptureFactory::registerParameters(ParamSection *sec) {
	sec->addBoolParam("useFlyCapture", &use, true);
};

VideoSource *FlyCaptureFactory::construct() {
	if (use) {
		FlyCaptureVideoSource *vs = new FlyCaptureVideoSource();
		if (vs->initialize()) return vs;
		delete vs;
	}
	return 0;
}

FlyCaptureVideoSource::FlyCaptureVideoSource()
{
	// create the flycapture context.
	flycaptureCreateContext( &context );
	playing = true;
	imagesExist = false;
}



bool FlyCaptureVideoSource::initialize()
{	
	// initialize the first camera on the bus.
	FlyCaptureError error = flycaptureInitialize( context, 0 );
	if(error == FLYCAPTURE_FAILED)
	{
		return false;
	} else {

		// start transfering images from the camera to this program
		error = flycaptureStart(context, FLYCAPTURE_VIDEOMODE_ANY, FLYCAPTURE_FRAMERATE_30);

		if(error != FLYCAPTURE_OK){								// if something is wrong
			return false;
		}
		///Get one image to have the size
		flycaptureGrabImage2(context,&pImage);
		int cameraWidth = pImage.iCols;
		int cameraHeight= pImage.iRows;
		tempImageColor = cvCreateImage(cvSize(cameraWidth, cameraHeight), IPL_DEPTH_8U, 3);
		tempImageGray = cvCreateImage(cvSize(cameraWidth, cameraHeight), IPL_DEPTH_8U, 1);
		
		imagesExist=true;
		return true;
	}
}

static void bayerDownsampleColor(const FlyCaptureImage *src, IplImage *dst)
{
	const unsigned char *srcpix = (const unsigned char *) src->pData;
	unsigned char *dstpix = (unsigned char *) dst->imageData;

	dst->channelSeq[0] = 'B';
	dst->channelSeq[1] = 'G';
	dst->channelSeq[2] = 'R';

	for (int y=0; y< dst->height; y++) {
		for (int x=0; x< dst->width; x++) {
			*dstpix++ = srcpix[0]; // red
			*dstpix++ = (srcpix[1] + srcpix[src->iRowInc]) >> 1; // green
			*dstpix++ = srcpix[src->iRowInc+1]; // blue

			//dstpix += 3; // skip alpha channel
			srcpix += 2;
		}
		srcpix += src->iRowInc;
	}
}

bool FlyCaptureVideoSource::getFrame(IplImage *dst) 
{
	if (!playing) return true;

	FlyCaptureImage fim;
	
	fim.iCols = dst->width;
	fim.iRows = dst->height;
	fim.iRowInc = dst->widthStep;
	fim.pData = (unsigned char*)dst->imageData;
	bool ok=true;

	if (dst->depth==IPL_DEPTH_8U) {
		switch(dst->nChannels) {
			case 1:	fim.pixelFormat = FLYCAPTURE_MONO8; break;
			case 3:	fim.pixelFormat = FLYCAPTURE_BGR; break;
			default:
				ok = false;
		}
	} else 
		ok =false;

	flycaptureGrabImage2(context,&pImage);
	FlyCaptureError error = flycaptureConvertImage(context, &pImage, &fim); 
	if(error != FLYCAPTURE_OK){								// if something is wrong
		std::cerr << "FlyCapture: Can't convert frame !\n";
		return false;
	}

	return true;
}



FlyCaptureVideoSource::~FlyCaptureVideoSource()
{
	flycaptureStop(context);
	flycaptureDestroyContext( context );
	if(imagesExist)
		cvReleaseImage(&tempImageColor);
	if(imagesExist)
		cvReleaseImage(&tempImageGray);

}

void FlyCaptureVideoSource::start() { 
	playing = true;
}

void FlyCaptureVideoSource::stop() {
	playing = false;
}

void FlyCaptureVideoSource::getSize(int &width, int &height)
{
	flycaptureGrabImage2(context,&pImage);
	width = pImage.iCols;
	height= pImage.iRows;
}