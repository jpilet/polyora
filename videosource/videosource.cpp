
//#include "stdafx.h"
#include "videosource.h"

#include "iniparser.h"

#ifdef WITH_DSHOWFILE
#	include "dshowfile.h"
#endif
#ifdef WITH_DSHOWCB
#	include "dshowcb.h"
#endif

#ifdef WITH_FLYCAPTURE
#	include "flycapturevs.h"
#endif
#ifdef WITH_FWCAM
#	include "fwcam.h"
#endif

#ifdef WITH_LINUXDV
#include "linuxdv.h"
#endif

#ifdef WITH_AVIDV
#include "avidv.h"
#endif

#include "bmpvideosource.h"

#include "opencv_vs.h"

#ifdef WITH_MPLAYER
#include "mplayer.h"
#endif

#ifdef WITH_PNGTRANSP
#include "pngtranspvs.h"
#endif

#ifdef WITH_DC1394
#include "dc1394vs.h"
#endif

VideoSourceFactory* VideoSourceFactory::factory=0;


void VideoSourceFactory::registerParameters(IniParser *parser) 
{
	ParamSection *sec = new ParamSection();
	sec->name = "VIDEO";
	parser->addSection(sec);

	registerParameters(sec);
}

void VideoSourceFactory::registerParameters(ParamSection *sec)
{
	for (FactoryVector::iterator it(factories.begin()); it != factories.end(); ++it) {
		(*it)->registerParameters(sec);
	}
}

VideoSource *VideoSourceFactory::construct() {
	for (FactoryVector::iterator it(factories.begin()); it != factories.end(); ++it) {
		VideoSource *s= (*it)->construct();
		if (s) return s;
	}
	return 0;
}

// Singleton implementation
VideoSourceFactory *VideoSourceFactory::instance() {
	if (!factory) factory = new VideoSourceFactory();
	return factory;
}

VideoSourceFactory::VideoSourceFactory() {

	registerFactory( new BmpFactory() );

#ifdef WITH_DSHOWFILE
	registerFactory( new DShowFileFactory() );
#endif

#ifdef WITH_DSHOWCB
	registerFactory( new DShowCBFactory() );
#endif

#ifdef WITH_FLYCAPTURE
	registerFactory( new FlyCaptureFactory() );
#endif

#ifdef WITH_AVIDV
	registerFactory( new AviDVFactory() );
#endif

#ifdef WITH_FWCAM
	registerFactory( new FWCamFactory() );
#endif

#ifdef WITH_DC1394
	registerFactory( new DC1394Factory() );
#endif

#ifdef WITH_LINUXDV
	registerFactory( new LinuxDVFactory() );
#endif

#ifdef WITH_MPLAYER
	registerFactory( new MPlayerFactory() );
#endif

	registerFactory( new OpenCVFactory() );

#ifdef WITH_PNGTRANSP
	registerFactory( new PngTranspFactory() );
#endif

}

VideoSourceFactory::~VideoSourceFactory() {

	for (FactoryVector::iterator it(factories.begin()); it != factories.end(); ++it) {
		delete *it;
	}
}

