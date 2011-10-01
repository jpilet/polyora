/*! 
  \defgroup videosource Videosource: Video acquisition abstraction
  \author Julien Pilet

  These classes provide a simple but flexible abstraction for video
  acquisition, with run-time configuration stored in a file.
*/
/*!@{*/

#ifndef __VIDEOSOURCE_H
#define __VIDEOSOURCE_H

//#include <cv.h>
//#include <opencv/cv.h>
//#include <cv.h>
#ifdef OPENCV_APPLE_FRAMEWORK
#include <OpenCV/OpenCV.h>
#else
#include <cv.h>
#include <highgui.h>
#endif
#include <vector>


#ifndef DLLEXPORT
#if 0
#ifdef WIN32
#define DLLEXPORT __declspec(dllexport)
#else 
#define DLLEXPORT
#endif
#else
#define DLLEXPORT
#endif
#endif

class FptFrame;
class ParamSection;
class IniParser;

//! Video source abstraction class.
/*! This class represent a video source, providing a sequence of images.
 *  In order to use a VideoSource object, initialize() has to be call and has
 *  to return true.
 *
 *  To construct a VideoSource object, it is a good idea to use the
 *  VideoSourceFactory class.
 */
class VideoSource 
{
public:

	/*! initialized the video source. Has to be called before any other
	 * method.
	 * \return true on success, false on error.
	 */
	virtual bool initialize() =0;

	/*! get the next frame and convert it to the format described in "dst".
	 * The image will be deinterlaced, scaled, and color converted to match
	 * dst.
	 * \param dst the image to copy the frame into.
	 * \return true on success, false on error.
	 */
	virtual bool getFrame(IplImage *dst)=0;

	/*! return width and height of images coming from the video source.
	 */
	virtual void getSize(int &width, int &height)=0;
	virtual ~VideoSource(){};

	/*! Start the video source: getFrame() will provide each time a different image.
	 * After calling initialize(), the video source is playing.
	 */
	virtual void start()=0;

	/*! Pause the source. The getFrame() method will always provide the current frame.
	 */
	virtual void stop()=0;

	//! Restart the video. On live media, just "start" it.
	virtual void restart() { start(); }

	/*! return false if the source is paused.
	 */
	virtual bool isPlaying()=0;

	/*! This method is usefull only for video files, not grabbing hardware.
	 * It returns a the number of the last frame provided by getFrame().
	 * \return the unique frame number or -1 if the video source does not support frame counting.
	 */
	virtual int getId() { return -1; }; 

	/*! Get the number of channels the video source provides.
	 * \return 1 for gray level images, 3 for RGB images.
	 */
	virtual int getChannels(){return 3;}

	/*! Allow the video source to have access to tracked data. Usefull for networked applications.
	 */
	virtual void postProcess(FptFrame *) {}

	/*! Returns a string describing the stream. If the source comes from a
	 * file, the filename is returned.
	 */
	virtual const char *getStreamName() const = 0;

	/*! Returns a string describing the type of the stream.
	 * For example: BmpFile, AviFile, DShowCB...
	 */
	virtual const char *getStreamType() const = 0;

        virtual void *getInternalPointer() { return 0; }
};

//! virtual class used to represent a particular video source factory.
/*! Each different VideoSource should have a factory deriving from
 * ParticularVSFactory, to help VideoSourceFactory to implement an
 * autodetection system.
 */
class ParticularVSFactory {
public:
	virtual ~ParticularVSFactory(){}
	const char *name; //< video source name
	virtual void registerParameters(ParamSection *sec)=0;
	//! returns 0 on failure.
	virtual VideoSource *construct()=0;
};


//! The VideoSourceFactory class constructs valid VideoSource objects.
/*! Basically, it is just a directory of ParticularVSFactory and redirect
 * registerParameters() and construct() calls to every factories.
 *
 * The registerParameters allow each driver to add it's own keywords in the
 * .ini file grammar. This call is optional, but provide runtime user
 * configuration.
 *
 * Here's how to use it:
 *
 * VideoSourceFactory::instance()->registerParameters(&iniParser);
 * VideoSource *vs = VideoSourceFactory::instance()->construct();
 *
 * if (vs ==0) {
 *	// fail
 * }
 * // the vs object is valid and ready.
 *
 *
 * To add a new video source type, open videosource.cpp, look for the
 * constructor, and call registerFactory().
 */
class VideoSourceFactory {
public:
	DLLEXPORT VideoSourceFactory();
	DLLEXPORT ~VideoSourceFactory();

	/*! Singleton implementation; gives access to the only
	 * VideoSourceFactory object.
	 */
	 DLLEXPORT  static VideoSourceFactory *instance();

	/*! The registerParameters allow each driver to add it's own keywords
	 * in the .ini file grammar. This call is optional, but provide runtime
	 * user configuration. Two versions of this method are available: one
	 * creates a new section, and the other one uses an existing section.
	 */
	DLLEXPORT void registerParameters(IniParser *parser);

	//! register parameters into an existing ParamSection
	DLLEXPORT void registerParameters(ParamSection *sec);

	//! construct a valid and working VideoSource
	/*! This method try to initialize all the registered drivers until a
	 * valid VideoSource is obtained.
	 * \return a working VideoSource or 0 on failure.
	 */
	DLLEXPORT VideoSource *construct();

	/*! register a VideoSource driver.
	 * calls to registerFactory are located in the VideoSourceFactory
	 * constructor, called once by instance().
	 */
	DLLEXPORT void registerFactory(ParticularVSFactory *f) { factories.push_back(f); }

private:

	typedef std::vector<ParticularVSFactory *> FactoryVector;
	static VideoSourceFactory *factory;
	FactoryVector factories;
};

/*!@}*/
#endif
