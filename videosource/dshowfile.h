//! \ingroup videosource
/*!@{*/
#ifndef __DSHOWFILE_H
#define __DSHOWFILE_H

#include <atlbase.h>
#include <windows.h>
#include <dshow.h>
#include <qedit.h>

#include "videosource.h"

//! Implementation of CSampleGrabberCB object
//
//! Note: this object is a SEMI-COM object, and can only be created statically.

class CSampleGrabberCB : public ISampleGrabberCB 
{

public:

    // These will get set by the main thread below. We need to
    // know this in order to write out the bmp
    long Width;
    long Height;
	IplImage *image;
	bool frameWanted;

	CSampleGrabberCB() {
		image=0;
		frameWanted=true;
	}

    // Fake out any COM ref counting
    //
    STDMETHODIMP_(ULONG) AddRef() 
	{ 
		return 1; 
	}

    STDMETHODIMP_(ULONG) Release() 
	{ 
		return 2; 
	}

    // Fake out any COM QI'ing
    //
    STDMETHODIMP QueryInterface(REFIID riid, void ** ppv)
    {
        if (!ppv) return E_POINTER;
        
        if( riid == IID_ISampleGrabberCB || riid == IID_IUnknown ) 
        {
            *ppv = (void *) static_cast<ISampleGrabberCB*> ( this );
            return NOERROR;
        }    

        return E_NOINTERFACE;
    }


    // We don't implement this one
    //
    STDMETHODIMP SampleCB( double SampleTime, IMediaSample * pSample )
    {
        return 0;
    }


    // The sample grabber is calling us back on its deliver thread.
    // This is NOT the main app thread!
    //
    STDMETHODIMP BufferCB( double SampleTime, BYTE * pBuffer, long BufferSize )
    {
		while (!frameWanted) {
			Sleep(2);
		};
		if (BufferSize > image->imageSize) {
			fprintf(stderr, __FILE__ ": error ! image size too big !!\n");
			return 0;
		}

		memcpy(image->imageData, pBuffer, BufferSize);
		cvFlip(image);	
		frameWanted=false;

		// cvSaveImage("bufferCB.bmp", image);
		//cvShowImage("Image", image); ////////////////
		////// cvWaitKey(1);
		//cvWaitKey(0); ////////////////

        return 0;
    }
};


class DShowFile : public VideoSource {
public:
	virtual bool initialize();
	virtual bool getFrame(IplImage *dst);
	virtual void getSize(int &width, int &height);
	DShowFile(const char *filename);
	virtual ~DShowFile();
	virtual void start();
	virtual void stop();
	virtual bool isPlaying() { return g_psCurrent == Playing; };
	virtual int getId() { return (int) lastFrame; };
	virtual const char *getStreamName() const { return filename ? filename : "DShow live capture"; }
	virtual const char *getStreamType() const { return "DShowFile"; }

private:
	enum PLAYSTATE {Stopped, Paused, Playing};

  	CSampleGrabberCB CB;

    CComPtr< ISampleGrabber > pGrabber;
    CComPtr< IBaseFilter >    pSource;
    CComPtr< IGraphBuilder >  pGraph;
    CComPtr< IVideoWindow >   pVideoWindow;
	IMediaSeeking *pSeeking;

	PLAYSTATE g_psCurrent;
	const char *filename;

	IplImage *image;

	int frameCnt;
	
	LONGLONG duration, lastFrame;
};

class DShowFileFactory : public ParticularVSFactory {
public:
	DShowFileFactory() { name="DShowFile"; filename=0; };
	virtual void registerParameters(ParamSection *sec);
	virtual VideoSource *construct();
private:
	char *filename;
	bool use;
};

void deinterlaceAndResize(IplImage *src, IplImage *dst);

#endif
/*!@}*/
