
#include "stdafx.h"

#define _WIN32_WINNT 0x400

#include <atlbase.h>
#include <windows.h>
#include <dshow.h>
#include <qedit.h>
#include <objbase.h>

#include <stdio.h>
#include <stdlib.h>

#include "dshowcb.h"

#include "iniparser.h"

const LONGLONG MILLISECONDS = (1000);            // 10 ^ 3
const LONGLONG NANOSECONDS = (1000000000);       // 10 ^ 9
const LONGLONG UNITS = (NANOSECONDS / 100);      // 10 ^ 7

void DShowCBFactory::registerParameters(ParamSection *sec) {
	sec->addBoolParam("useDShowCB", &use, true);
};

VideoSource *DShowCBFactory::construct() {
	if (use) {
		DShowCB *vs = new DShowCB();
		if (vs->initialize()) return vs;
		delete vs;
	}
	return 0;
}

//
// Implementation of CSampleGrabberCB object
//
// Note: this object is a SEMI-COM object, and can only be created statically.

class SampleGrabberCB : public ISampleGrabberCB 
{

public:

    // These will get set by the main thread below. We need to
    // know this in order to write out the bmp
    long Width;
    long Height;
	IplImage *image;
	int frameId;
	bool frameWanted;
	HANDLE event;

    // Fake out any COM ref counting
    //
    STDMETHODIMP_(ULONG) AddRef() { return 2; }
    STDMETHODIMP_(ULONG) Release() { return 1; }

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
		frameId++;
		if (!frameWanted) return 0;

		if (BufferSize > image->imageSize) {
			fprintf(stderr, __FILE__ ": error ! image size too big !!\n");
			return 0;
		}

		memcpy(image->imageData, pBuffer, BufferSize);
		frameWanted=false;
		SetEvent(event);
        return 0;
    }
};

// This semi-COM object will receive sample callbacks for us
//
SampleGrabberCB CB;

// Function prototypes
int GrabBitmaps(TCHAR * szFile);
HRESULT GetPin(IBaseFilter * pFilter, PIN_DIRECTION dirrequired,  int iNum, IPin **ppPin);
IPin *  GetInPin ( IBaseFilter *pFilter, int Num );
IPin *  GetOutPin( IBaseFilter *pFilter, int Num );

DShowCB::DShowCB()
{
	image =0;
	frameCnt=0;
	g_psCurrent = Stopped;
}

DShowCB::~DShowCB()
{
	m_pCapture->Release();
	m_pMC->Release();
	if (image) cvReleaseImage(&image);
}

bool DShowCB::initialize()
{
    HRESULT hr;

    // Initialize COM
    if(FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)))
    {
        fprintf(stderr, "CoInitialize Failed!\r\n");   
        //exit(1);
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

    // Create the graph
    //
    pGraph.CoCreateInstance( CLSID_FilterGraph );
    if( !pGraph )
    {
        fprintf(stderr, "Could not not create the graph\r\n");
        return false;
    }
    // Create the capture graph builder
    hr = CoCreateInstance (CLSID_CaptureGraphBuilder2 , NULL, CLSCTX_INPROC,
                           IID_ICaptureGraphBuilder2, (void **) &m_pCapture);
    if (FAILED(hr)) return false;
    
    // Obtain interfaces for media control and Video Window
    hr = pGraph->QueryInterface(IID_IMediaControl,(LPVOID *) &m_pMC);
    if (FAILED(hr)) return false;

    // Attach the filter graph to the capture graph
    hr = m_pCapture->SetFiltergraph(pGraph);
    if (FAILED(hr))
    {
        fprintf(stderr, "Failed to set capture filter graph!  hr=0x%x", hr);
        return false;
    }

    // Use the system device enumerator and class enumerator to find
    // a video capture/preview device, such as a desktop USB video camera.
    IBaseFilter *pSrcFilter=NULL;
    hr = FindCaptureDevice(&pSrcFilter);
    if (FAILED(hr)) return false;
   
    // Add Capture filter to our graph.
    hr = pGraph->AddFilter(pSrcFilter, L"Video Capture");
    if (FAILED(hr))
    {
        fprintf(stderr, "Couldn't add the capture filter to the graph!  hr=0x%x\r\n\r\n"
            "If you have a working video capture device, please make sure\r\n"
            "that it is connected and is not being used by another application.\r\n\r\n", hr);
        pSrcFilter->Release();
        return false;
    }

    // Put them in the graph
    //
    hr = pGraph->AddFilter( pGrabberBase, L"Grabber" );

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

    pSourcePin = GetOutPin( pSrcFilter, 0 );
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
	CB.image = image = cvCreateImage(cvSize(CB.Width, CB.Height), IPL_DEPTH_8U, 3);

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
    hr = pGrabber->SetOneShot( FALSE );

    // Set the callback, so we can grab the one sample
    //
    hr = pGrabber->SetCallback( &CB, 1 );

    // Query the graph for the IVideoWindow interface and use it to
    // disable AutoShow.  This will prevent the ActiveMovie window from
    // being displayed while we grab bitmaps from the running movie.
    CComQIPtr< IVideoWindow, &IID_IVideoWindow > pWindow = pGraph;
    if (pWindow)
    {
        hr = pWindow->put_AutoShow(OAFALSE);
    }

	CB.event = CreateEvent(0, FALSE, FALSE, _T("newFrameEvent"));
	if (CB.event == NULL) {
		fprintf(stderr, "CreateEvent failed!\n");
		return false;
	}

	pSrcFilter->Release();
	pSrcFilter=0;

	start();

	return true;
}

bool DShowCB::getFrame(IplImage *dst) {
/*
	if (g_psCurrent != Paused) {

		if (g_psCurrent == Stopped)
			g_psCurrent = Paused;
					
		// callback wrote the image. Resize it.
	}
*/
	CB.frameWanted = true;
	// TODO: Lock in order to avoid callback conflict !

	WaitForSingleObject(CB.event, 100);
	deinterlaceAndResize(image, dst);
	
	return true;
}


void DShowCB::getSize(int &width, int &height)
{
	width = CB.Width;
	height = CB.Height;
}

void DShowCB::start()
{
	g_psCurrent = Playing;
	m_pMC->Run();
}

void DShowCB::stop()
{
	g_psCurrent = Paused;
	m_pMC->Pause();
}

static HRESULT GetPin( IBaseFilter * pFilter, PIN_DIRECTION dirrequired, int iNum, IPin **ppPin)
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


static IPin * GetInPin( IBaseFilter * pFilter, int nPin )
{
    CComPtr<IPin> pComPin=0;
    GetPin(pFilter, PINDIR_INPUT, nPin, &pComPin);
    return pComPin;
}


static IPin * GetOutPin( IBaseFilter * pFilter, int nPin )
{
    CComPtr<IPin> pComPin=0;
    GetPin(pFilter, PINDIR_OUTPUT, nPin, &pComPin);
    return pComPin;
}


static void deinterlaceAndResize(IplImage *image, IplImage *realDst) 
{
	
	static IplImage *tmpIm = 0;
	
	if (tmpIm==0)
		tmpIm = cvCreateImage(cvGetSize(realDst), IPL_DEPTH_8U, 3);

	IplImage *dst = realDst;

	// check if color -> gray conversion is required
	if (image->nChannels != realDst->nChannels) 
		dst = tmpIm;
/*
	// deinterlace + flip vertically
	for(int y = 0; y < tmpIm->height; y++)
	{
		//int srcY = (src->height-1 - 2*y);
		int srcY = y*2;

		memcpy(tmpIm->imageData + y*tmpIm->widthStep, src->imageData + srcY*src->widthStep,
			src->widthStep);
	}

	cvResize(tmpIm, dst);
	*/
	if ((dst->width == (image->width/2)) && (dst->height == (image->height/2))) {
		// fast deinterlacing + resizing + flip
		for(int y = 0; y < dst->height; y++)
		{
			unsigned char *srcpix = ((unsigned char *)(image->imageData)) + 2*y*image->widthStep;
			unsigned char *dstpix = ((unsigned char *)(dst->imageData)) + (dst->height-1 - y)*dst->widthStep;
			if (image->nChannels == 1) {
				for (int x=0; x < dst->width; ++x) {
					*dstpix++ = unsigned char((int(srcpix[0])+int(srcpix[1]))>>1);
					srcpix+=2;
				}
			} else {
				for (int x=0; x < dst->width; ++x) {
					*dstpix++ = unsigned char((int(srcpix[0])+int(srcpix[3]))>>1);
					*dstpix++ = unsigned char((int(srcpix[1])+int(srcpix[4]))>>1);
					*dstpix++ = unsigned char((int(srcpix[2])+int(srcpix[5]))>>1);
					srcpix+=6;
				}
			}
		}
	} else {
		static IplImage *tmpIm = 0;
		if (tmpIm==0)
			tmpIm = cvCreateImage(cvSize(image->width, image->height/2), IPL_DEPTH_8U, 3);

		// deinterlace + flip vertically
		for(int y = 0; y < tmpIm->height; y++)
		{
			int srcY = (image->height-1 - 2*y);

			memcpy(tmpIm->imageData + y*tmpIm->widthStep, image->imageData + srcY*image->widthStep,
				image->widthStep);
		}

		cvResize(tmpIm, dst);
	}
	if (image->nChannels != realDst->nChannels) 
		cvCvtColor(tmpIm, realDst, CV_RGB2GRAY);
}

HRESULT DShowCB::FindCaptureDevice(IBaseFilter ** ppSrcFilter)
{
    HRESULT hr;
    IBaseFilter * pSrc = NULL;
    CComPtr <IMoniker> pMoniker =NULL;
    ULONG cFetched;

    if (!ppSrcFilter)
        return E_POINTER;
   
    // Create the system device enumerator
    CComPtr <ICreateDevEnum> pDevEnum =NULL;

    hr = CoCreateInstance (CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC,
                           IID_ICreateDevEnum, (void **) &pDevEnum);
    if (FAILED(hr))
    {
        fprintf(stderr, "Couldn't create system enumerator!  hr=0x%x", hr);
        return hr;
    }

    // Create an enumerator for the video capture devices
    CComPtr <IEnumMoniker> pClassEnum = NULL;

    hr = pDevEnum->CreateClassEnumerator (CLSID_VideoInputDeviceCategory, &pClassEnum, 0);
    if (FAILED(hr))
    {
        fprintf(stderr, "Couldn't create class enumerator!  hr=0x%x", hr);
        return hr;
    }

    // If there are no enumerators for the requested type, then 
    // CreateClassEnumerator will succeed, but pClassEnum will be NULL.
    if (pClassEnum == NULL)
    {
        fprintf(stderr,"No Video Capture Hardware found\n");
        return E_FAIL;
    }

    // Use the first video capture device on the device list.
    // Note that if the Next() call succeeds but there are no monikers,
    // it will return S_FALSE (which is not a failure).  Therefore, we
    // check that the return code is S_OK instead of using SUCCEEDED() macro.
    if (S_OK == (pClassEnum->Next (1, &pMoniker, &cFetched)))
    {
        // Bind Moniker to a filter object
        hr = pMoniker->BindToObject(0,0,IID_IBaseFilter, (void**)&pSrc);
        if (FAILED(hr))
        {
            fprintf(stderr, "Couldn't bind moniker to filter object!  hr=0x%x", hr);
            return hr;
        }
    }
    else
    {
        fprintf(stderr,"Unable to access video capture device!\n");   
        return E_FAIL;
    }

    // Copy the found filter pointer to the output parameter.
    // Do NOT Release() the reference, since it will still be used
    // by the calling function.
    *ppSrcFilter = pSrc;

    return hr;
}

