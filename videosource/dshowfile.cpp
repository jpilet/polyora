#include "stdafx.h"



#include <atlbase.h>
#include <windows.h>
#include <dshow.h>
#include <qedit.h>
#include <objbase.h>

#include <stdio.h>
#include <stdlib.h>

#include "dshowfile.h"

#include "iniparser.h"

const LONGLONG MILLISECONDS = (1000);            // 10 ^ 3
const LONGLONG NANOSECONDS = (1000000000);       // 10 ^ 9
const LONGLONG UNITS = (NANOSECONDS / 100);      // 10 ^ 7

void DShowFileFactory::registerParameters(ParamSection *sec) {
	sec->addStringParam("aviFile", &filename, "default.avi");
	sec->addBoolParam("useDShowFile", &use, true);
};

VideoSource *DShowFileFactory::construct() {
	if (use) {
		DShowFile *vs = new DShowFile(filename);
		if (vs->initialize()) return vs;
		delete vs;
	}
	return 0;
}

////
//// Implementation of CSampleGrabberCB object
////
//// Note: this object is a SEMI-COM object, and can only be created statically.
//
//class CSampleGrabberCB : public ISampleGrabberCB 
//{
//
//public:
//
//    // These will get set by the main thread below. We need to
//    // know this in order to write out the bmp
//    long Width;
//    long Height;
//	IplImage *image;
//
//	CSampleGrabberCB() { image=0; }
//
//    // Fake out any COM ref counting
//    //
//    STDMETHODIMP_(ULONG) AddRef() { return 2; }
//    STDMETHODIMP_(ULONG) Release() { return 1; }
//
//    // Fake out any COM QI'ing
//    //
//    STDMETHODIMP QueryInterface(REFIID riid, void ** ppv)
//    {
//        if (!ppv) return E_POINTER;
//        
//        if( riid == IID_ISampleGrabberCB || riid == IID_IUnknown ) 
//        {
//            *ppv = (void *) static_cast<ISampleGrabberCB*> ( this );
//            return NOERROR;
//        }    
//
//        return E_NOINTERFACE;
//    }
//
//
//    // We don't implement this one
//    //
//    STDMETHODIMP SampleCB( double SampleTime, IMediaSample * pSample )
//    {
//        return 0;
//    }
//
//
//    // The sample grabber is calling us back on its deliver thread.
//    // This is NOT the main app thread!
//    //
//    STDMETHODIMP BufferCB( double SampleTime, BYTE * pBuffer, long BufferSize )
//    {
//		if (BufferSize > image->imageSize) {
//			fprintf(stderr, __FILE__ ": error ! image size too big !!\n");
//			return 0;
//		}
//
//		memcpy(image->imageData, pBuffer, BufferSize);
//		cvFlip(image);
//        return 0;
//    }
//};
//
//// This semi-COM object will receive sample callbacks for us
////! TODO: get rid of this global var.
//CSampleGrabberCB CB;

// Function prototypes
int GrabBitmaps(TCHAR * szFile);
HRESULT GetPin(IBaseFilter * pFilter, PIN_DIRECTION dirrequired,  int iNum, IPin **ppPin);
IPin *  GetInPin ( IBaseFilter *pFilter, int Num );
IPin *  GetOutPin( IBaseFilter *pFilter, int Num );

DShowFile::DShowFile(const char *filename)
{
	image =0;
	this->filename = filename;
	frameCnt=0;
	g_psCurrent = Stopped;
}

DShowFile::~DShowFile()
{
	if (image) cvReleaseImage(&image);
}

bool DShowFile::initialize()
{
	if (!filename) return false;

    HRESULT hr;

    // Initialize COM
    if(FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)))
    {
        fprintf(stderr, "CoInitialize Failed!\r\n");   
        // return false;
    } 

    // Create the sample grabber
    //
    pGrabber.CoCreateInstance( CLSID_SampleGrabber );
    if( !pGrabber )
    {
        fprintf(stderr, "Could not create CLSID_SampleGrabber\r\n");
        return false;
    }
    CComQIPtr< IBaseFilter, &IID_IBaseFilter > pGrabberBase( pGrabber );

    // Create the file reader
    //
    pSource.CoCreateInstance( CLSID_AsyncReader );
    if( !pSource )
    {
        fprintf(stderr, "Could not create source filter\r\n");
        return false;
    }

    // Create the graph
    //
    pGraph.CoCreateInstance( CLSID_FilterGraph );
    if( !pGraph )
    {
        fprintf(stderr, "Could not not create the graph\r\n");
        return false;
    }

    // Put them in the graph
    //
    hr = pGraph->AddFilter( pSource, L"Source" );
    hr = pGraph->AddFilter( pGrabberBase, L"Grabber" );

    // Load the source
    //
	wchar_t wfn[256];
	mbstowcs(wfn, filename, 256);
    CComQIPtr< IFileSourceFilter, &IID_IFileSourceFilter > pLoad( pSource );
    hr = pLoad->Load( wfn, NULL );
    if( FAILED( hr ) )
    {
        fprintf(stderr, "Could not load the media file\r\n");
        return false;
    }

    // Tell the grabber to grab 24-bit video. Must do this
    // before connecting it
    //
	//pGrabberF->QueryInterface(IID_ISampleGrabber, (void**)&pGrabber);
	AM_MEDIA_TYPE mt;
	ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));
	mt.majortype = MEDIATYPE_Video;
	mt.subtype = MEDIASUBTYPE_RGB24;
	hr = pGrabber->SetMediaType(&mt);
	if (FAILED(hr)) return false;


    // Get the output pin and the input pin
    //
    CComPtr< IPin > pSourcePin;
    CComPtr< IPin > pGrabPin;

    pSourcePin = GetOutPin( pSource, 0 );
    pGrabPin   = GetInPin( pGrabberBase, 0 );

    // ... and connect them
    //
    hr = pGraph->Connect( pSourcePin, pGrabPin );
    if( FAILED( hr ) )
    {
        fprintf(stderr, "Could not connect source filter to grabber\r\n");
        return false;
    }

    // Ask for the connection media type so we know its size
    //
    hr = pGrabber->GetConnectedMediaType( &mt );

    VIDEOINFOHEADER * vih = (VIDEOINFOHEADER*) mt.pbFormat;
    CB.Width  = vih->bmiHeader.biWidth;
    CB.Height = vih->bmiHeader.biHeight;

	if (CB.image==0) {
		CB.image = image = cvCreateImage(cvSize(CB.Width, CB.Height), IPL_DEPTH_8U, 3);
	} else {
		if (CB.Width != CB.image->width || CB.Height != CB.image->height) {
			std::cerr << "BUG! Fixme! dshowfile.cpp!!!\n";
			return false;
		}
		image = CB.image;
	}

    // Render the grabber output pin (to a video renderer)
    //
    CComPtr <IPin> pGrabOutPin = GetOutPin( pGrabberBase, 0 );
    hr = pGraph->Render( pGrabOutPin );
    if( FAILED( hr ) )
    {
        fprintf(stderr, "Could not render grabber output pin\r\n");
        return false;
    }

    // Don't buffer the samples as they pass through
    //
    hr = pGrabber->SetBufferSamples( FALSE );

    // Only grab one at a time, stop stream after
    // grabbing one sample
    //
    hr = pGrabber->SetOneShot( TRUE );

    // Set the callback, so we can grab the one sample
    //
    hr = pGrabber->SetCallback( &CB, 1 );

    // Get the seeking interface, so we can seek to a location
    //
	hr = pGraph->QueryInterface(IID_IMediaSeeking, (LPVOID *) &pSeeking);
	if (FAILED(hr)) {
		fprintf(stderr, "cannot query media seeking interface !\n");
		return false;
	}

	hr = pSeeking->SetTimeFormat(&TIME_FORMAT_FRAME);
	if (FAILED(hr)) {
		fprintf(stderr, "cannot change time format !\n");
		return false;
	}

	hr = pSeeking->GetDuration(&duration);
	if (FAILED(hr)) {
		fprintf(stderr, "cannot get video duration !\n");
		return false;
	}

	printf("%s: %dx%d, %d samples.\n", filename, CB.Width, CB.Height, int(duration));

    // Query the graph for the IVideoWindow interface and use it to
    // disable AutoShow.  This will prevent the ActiveMovie window from
    // being displayed while we grab bitmaps from the running movie.
    CComQIPtr< IVideoWindow, &IID_IVideoWindow > pWindow = pGraph;
    if (pWindow)
    {
        hr = pWindow->put_AutoShow(OAFALSE);
    }
	return true;
}

bool DShowFile::getFrame(IplImage *dst) {

	if (g_psCurrent != Paused) {

		if (g_psCurrent == Stopped)
			g_psCurrent = Paused;

		// set position
		lastFrame = frameCnt;
		HRESULT hr;
		//HRESULT hr = pSeeking->SetPositions( &lastFrame, AM_SEEKING_AbsolutePositioning, 
		//	NULL, AM_SEEKING_NoPositioning );

		if (lastFrame >= duration) {
			frameCnt=0;
			stop();
		} else {
			frameCnt++;
		}
		
		// activate the threads
		CComQIPtr< IMediaControl, &IID_IMediaControl > pControl( pGraph );
		hr = pControl->Run( );
		
		// wait for the graph to settle
		CComQIPtr< IMediaEvent, &IID_IMediaEvent > pEvent( pGraph );
		long EvCode = 0;
		
		hr = pEvent->WaitForCompletion( INFINITE, &EvCode );
	
		// callback wrote the image. Resize it.

		CB.frameWanted = true;
	}

	while (CB.frameWanted) {
		Sleep(2);
	};
	
	cvCopy(image,dst);
	//deinterlaceAndResize(image, dst);
	
	return true;
}


void DShowFile::getSize(int &width, int &height)
{
	width = CB.Width;
	height = CB.Height;
}

void DShowFile::start()
{
	g_psCurrent = Playing;
}

void DShowFile::stop()
{
	g_psCurrent = Paused;
}

HRESULT GetPin( IBaseFilter * pFilter, PIN_DIRECTION dirrequired, int iNum, IPin **ppPin)
{
    CComPtr< IEnumPins > pEnum;
    *ppPin = NULL;

    HRESULT hr = pFilter->EnumPins(&pEnum);
    if(FAILED(hr)) 
        return hr;

    ULONG ulFound;
    IPin *pPin;
    hr = E_FAIL;

    while(S_OK == pEnum->Next(1, &pPin, &ulFound))
    {
        PIN_DIRECTION pindir = (PIN_DIRECTION)3;

        pPin->QueryDirection(&pindir);
        if(pindir == dirrequired)
        {
            if(iNum == 0)
            {
                *ppPin = pPin;  // Return the pin's interface
                hr = S_OK;      // Found requested pin, so clear error
                break;
            }
            iNum--;
        } 

        pPin->Release();
    } 

    return hr;
}


IPin * GetInPin( IBaseFilter * pFilter, int nPin )
{
    CComPtr<IPin> pComPin=0;
    GetPin(pFilter, PINDIR_INPUT, nPin, &pComPin);
    return pComPin;
}


IPin * GetOutPin( IBaseFilter * pFilter, int nPin )
{
    CComPtr<IPin> pComPin=0;
    GetPin(pFilter, PINDIR_OUTPUT, nPin, &pComPin);
    return pComPin;
}


static void deinterlaceResizeHalf(const IplImage *src, IplImage *dst)
{
        assert(src->depth == IPL_DEPTH_8U);

        // fast deinterlacing + resizing
        for(int y = 0; y < dst->height; y++)
        {
                unsigned char *srcpix = ((unsigned char *)(src->imageData)) + 2*y*src->widthStep;
                unsigned char *dstpix = ((unsigned char *)(dst->imageData)) + y*dst->widthStep;
                for (int x=0; x < dst->width; ++x) {
                        if ((src->nChannels == 1) && (dst->nChannels == 1)) {
                                *dstpix++ = (unsigned char)((int(srcpix[0])+int(srcpix[1]))>>1);
                                srcpix+=2;
                        } else if (dst->nChannels==1 && src->nChannels==3) {
                                *dstpix++ = (unsigned char)((((int(srcpix[0])+int(srcpix[3]))>>1) +
                                        ((int(srcpix[1])+int(srcpix[4]))>>1) +
                                        ((int(srcpix[2])+int(srcpix[5]))>>1) )/3);
                                srcpix+=6;
						} else if (dst->nChannels==3 && src->nChannels==3) {
                                *dstpix++ = (unsigned char)((int(srcpix[0])+int(srcpix[3]))>>1);
                                *dstpix++ = (unsigned char)((int(srcpix[1])+int(srcpix[4]))>>1);
                                *dstpix++ = (unsigned char)((int(srcpix[2])+int(srcpix[5]))>>1);
								srcpix+=6;
						}
                }
        }
}

void deinterlaceAndResize(IplImage *src, IplImage *dst) 
{
	static IplImage *tmpIm = 0;
	
	if ((src->width/2) == dst->width && (src->height/2 == dst->height)) {
		deinterlaceResizeHalf(src, dst);
		return;
	}

	if (tmpIm==0)
		tmpIm = cvCreateImage(cvSize(src->width, src->height/2), IPL_DEPTH_8U, 3);

	// deinterlace + flip vertically
	for(int y = 0; y < tmpIm->height; y++)
	{
		//int srcY = (src->height-1 - 2*y);
		int srcY = y*2;

		// TODO: Color to gray conversion !
		memcpy(tmpIm->imageData + y*tmpIm->widthStep, src->imageData + srcY*src->widthStep,
			src->widthStep);
	}

	cvResize(tmpIm, dst);
}


