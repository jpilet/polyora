
#include <stdio.h>
#include <iostream>

#include "bmpvideosource.h"
#include "iniparser.h"

using namespace std;

void BmpFactory::registerParameters(ParamSection *sec) {
	sec->addStringParam("bmpFile", &filename, "default%04d.bmp");
	sec->addBoolParam("useBmpFile", &use, true);
	sec->addIntParam("bmp.first", &first, -1);
	sec->addIntParam("bmp.last", &last, -1);
};

VideoSource *BmpFactory::construct() {
	if (use) {
		BmpVideoSource *vs = new BmpVideoSource(filename, first, last);
		if (vs->initialize()) return vs;
		delete vs;
	}
	return 0;
}

BmpVideoSource::BmpVideoSource(char *genericFilename, int first, int last)
	: genericFilename(genericFilename)
{	
	firstImageIndex= first;
	lastImageIndex = last;
	frameCnt = firstImageIndex;
	width=height=-1;
}

bool BmpVideoSource::initialize() 
{
	char fn[256];
	FILE *f;

	if (firstImageIndex>=0)
		frameCnt =firstImageIndex;
	else 
		frameCnt = 0;

	int start = frameCnt;

	sprintf(fn, genericFilename, frameCnt);
	f=fopen(fn, "r");
	
	while(!f)
	{
		sprintf(fn, genericFilename, ++frameCnt);
		f=fopen(fn, "r");
		if((start+frameCnt) > 1000)
			break;
	}
	if (f) 
	{
		firstImageIndex = frameCnt;
		fclose(f);
		return true;
	} 

	return false;
}

bool BmpVideoSource::getFrame(IplImage *dst) 
{
	char fn[256];

	sprintf(fn, genericFilename, frameCnt);

	IplImage *im = cvLoadImage(fn, (dst->nChannels == 3 ? 1 : 0)); 

	if (im == 0)
	{
		if (frameCnt== firstImageIndex)
			{
				cout << "cannot open images from file";
				return false;
			}
		if (playing) {
			frameCnt++;

			if (frameCnt == lastImageIndex)
				frameCnt = firstImageIndex;
		}

		//return false;
		frameCnt = firstImageIndex;
		return getFrame(dst); // retry with frame #1
	} 
	//cout << fn << ": loaded.\n";

	width = im->width;
	height = im->height;

	if ((im->width == dst->width) && (im->height == dst->height)) 
		cvCopy(im,dst);
	else
		cvResize(im, dst);

	cvReleaseImage(&im);

	if (playing) {
		frameCnt++;

		if (frameCnt == lastImageIndex)
			frameCnt = firstImageIndex;
	}

	return true;
}

int BmpVideoSource::getId() 
{
	if (playing) return frameCnt-1;
	return frameCnt;
}

BmpVideoSource::~BmpVideoSource() {
}

void BmpVideoSource::start() {
	playing = true;
}

void BmpVideoSource::stop() {
	playing = false;
}


void BmpVideoSource::getSize(int &w, int &h)
{
	if (width<0 || height<0) {
		char fn[256];
		IplImage *im;

		do
		{
			sprintf(fn, genericFilename, frameCnt);
			im = cvLoadImage(fn, 8);
			if(im || frameCnt > 1000)
				break;
			frameCnt++;
		}
		while (true);

		assert(im);
		width = w = im->width;
		height = h = im->height;
		cvReleaseImage(&im);
	} else {
		w=width;
		h=height;
	}
}
