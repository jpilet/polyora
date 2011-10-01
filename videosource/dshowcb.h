//! \ingroup videosource
/*!@{*/
#ifndef __DSHOWCB_H
#define __DSHOWCB_H

#include <atlbase.h>
#include <windows.h>
#include <dshow.h>
#include <qedit.h>

#include "videosource.h"

class DShowCB : public VideoSource {
public:
	virtual bool initialize();
	virtual bool getFrame(IplImage *dst);
	virtual void getSize(int &width, int &height);
	DShowCB();
	virtual ~DShowCB();
	virtual void start();
	virtual void stop();
	virtual bool isPlaying() { return g_psCurrent == Playing; };
	virtual int getId() { return (int) lastFrame; };
	virtual const char *getStreamName() const { return filename ? filename : "DShow live capture"; }
	virtual const char *getStreamType() const { return "DShowCB"; }

private:
	enum PLAYSTATE {Stopped, Paused, Playing};

    CComPtr< ISampleGrabber > pGrabber;
    CComPtr< IGraphBuilder >  pGraph;
    CComPtr< IVideoWindow >   pVideoWindow;
	ICaptureGraphBuilder2 * m_pCapture;
	IMediaControl * m_pMC;

	PLAYSTATE g_psCurrent;
	const char *filename;

	IplImage *image;

	int frameCnt;
	
	LONGLONG duration, lastFrame;

	HRESULT FindCaptureDevice(IBaseFilter ** ppSrcFilter);
};

class DShowCBFactory : public ParticularVSFactory {
public:
	DShowCBFactory() { name="DShowCB"; };
	virtual void registerParameters(ParamSection *sec);
	virtual VideoSource *construct();
private:
	bool use;
};

void deinterlaceAndResize(IplImage *src, IplImage *dst);

#endif
/*!@}*/
