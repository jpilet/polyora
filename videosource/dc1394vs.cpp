
#include <stdio.h>
#include <iostream>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "dc1394vs.h"

using namespace std;

void DC1394Factory::registerParameters(ParamSection *sec) {
	sec->addBoolParam("useDC1394", &use, true);
	sec->addBoolParam("dc1394.debug", &params.debug, false);
	sec->addBoolParam("dc1394.gray", &params.gray, false);
        sec->addIntParam("dc1394.width", &params.width, -1);
        sec->addIntParam("dc1394.height", &params.height, -1);
        sec->addIntParam("dc1394.framerate", &params.framerate, -1);
        sec->addIntParam("dc1394.bus_speed", &params.bus_speed, -1);
};

VideoSource *DC1394Factory::construct() {
	if (use) {
		
		DC1394 *vs = new DC1394(params);
		if (vs->initialize()) return vs;
		delete vs;
	}
	return 0;
}

DC1394::DC1394(Params &p) : params(p) 
{
	image =0;
	playing = false;
	camera = 0;
	frame=0;
}

DC1394::~DC1394()
{
	stop();
	if (image) cvReleaseImage(&image);

	if (camera) {
		dc1394_video_set_transmission(camera, DC1394_OFF);
		dc1394_capture_stop(camera);
                //dc1394_camera_free(camera);
		dc1394_free (d);
	}
}

int DC1394::getChannels() {
	return channels;
}

static const char *color_coding_name(dc1394color_coding_t a)
{
    switch(a) {
          case DC1394_COLOR_CODING_MONO8: return "mono8";
          case DC1394_COLOR_CODING_YUV411 : return "yuv411";
          case DC1394_COLOR_CODING_YUV422 : return "yuv422";
          case DC1394_COLOR_CODING_YUV444 : return "yuv444";
          case DC1394_COLOR_CODING_RGB8 : return "rgb8";
          case DC1394_COLOR_CODING_MONO16 : return "mono16";
          case DC1394_COLOR_CODING_RGB16 : return "rgb16";
          case DC1394_COLOR_CODING_MONO16S : return "mono16s";
          case DC1394_COLOR_CODING_RGB16S : return "rgb16s";
          case DC1394_COLOR_CODING_RAW8 : return "raw8";
          case DC1394_COLOR_CODING_RAW16 : return "raw16";
          default: return "unknown color coding";
    }
}

ostream & print_mode(ostream &stream, dc1394camera_t *camera, const dc1394video_mode_t m) {
    uint32_t x,y;
    dc1394color_coding_t nColourCoding;
    dc1394_get_image_size_from_video_mode(camera, m, &x, &y);
    dc1394_get_color_coding_from_video_mode(camera,m,&nColourCoding);
    stream << x << "x" <<y << " " << color_coding_name(nColourCoding);
    return stream;
}

static void cleanup_and_exit(dc1394camera_t *camera)
{
    dc1394_video_set_transmission(camera, DC1394_OFF);
    dc1394_capture_stop(camera);
    dc1394_camera_free(camera);
}

bool DC1394::init_video_mode()
{
    // First, get a list of the modes which the camera supports.
    dc1394video_modes_t modes;
    dc1394error_t error = dc1394_video_get_supported_modes(camera, &modes);
    if(error) { cerr << "Could not get modelist\n"; return false; }

    const dc1394color_coding_t nColourCodings[] = {DC1394_COLOR_CODING_RGB8,DC1394_COLOR_CODING_MONO8};
    const int nChannels[] = {3,1};

    if (params.debug) {
        cout << "DC1394: possible video modes:" << endl;
    }

    // Second: Build up a list of the modes which are the right colour-space
    std::vector<dc1394video_mode_t> vModes;
    for (int i=0;vModes.size()==0 && i<2;i++) {
	    dc1394color_coding_t nTargetColourCoding = nColourCodings[i];
	    channels = nChannels[i];

	    for(unsigned int i = 0; i < modes.num; i++)
	    {
		    dc1394video_mode_t nMode = modes.modes[i];
		    // ignore format 7 for now
		    if(nMode >= DC1394_VIDEO_MODE_FORMAT7_0) continue;
		    dc1394color_coding_t nColourCoding;
		    error=dc1394_get_color_coding_from_video_mode(camera,nMode,&nColourCoding);
		    if(error) {
			    cerr << "Error in get_color_coding_from_video_mode\n";
			    return false;
		    }
		    if(nColourCoding == nTargetColourCoding) {
			    vModes.push_back(nMode);

			    if (params.debug) 
				    print_mode(cout << "\t", camera, nMode) << endl;
		    }
	    }
    }

    cout << "*** channels: " << channels << endl;

    if(vModes.size() == 0) {
        cerr << "No video mode found for camera!\n";
	dc1394_camera_free(camera);
	camera=0;
        return false;
    }

    // Third: Select mode according to size
    bool bModeFound = false;
    dc1394video_mode_t nMode;
    if(params.width>0)	{   // Has the user specified a target size? Choose mode according to that..
        for(size_t i = 0; i<vModes.size(); i++) {
            uint32_t x,y;
            dc1394_get_image_size_from_video_mode(camera, vModes[i], &x, &y);

            if(x == (uint32_t) params.width && (params.height<0 || y == (uint32_t) params.height)) {
                bModeFound = true;
                nMode = vModes[i];
                break;
            }
        }
        if(!bModeFound) {
            cerr << "Video mode " << params.width << "x" << params.height << " not found!\n";
        }
    }
    if (!bModeFound) {
        // If the user didn't specify a target size, choose the one with the
        // highest resolution.
        sort(vModes.begin(), vModes.end());
        bModeFound = true;
        nMode = vModes.back();
    }

    // Store the size of the selected mode..
    uint32_t x,y;
    dc1394_get_image_size_from_video_mode(camera, nMode, &x, &y);
    width = x;
    height = y;

    // Got mode, now decide on frame-rate. Similar thing: first get list, then choose from list.
    dc1394framerates_t framerates;
    dc1394framerate_t nChosenFramerate = DC1394_FRAMERATE_MIN;
    error = dc1394_video_get_supported_framerates(camera,nMode,&framerates);
    if(error) { cerr << "Could not query supported framerates" << endl; return false; }

    if (params.debug) {
        print_mode(cout << "Possible framerates for video mode ", camera, nMode) << ": ";
        for(unsigned int i=0; i<framerates.num; i++){
            float f_rate;
            dc1394_framerate_as_float(framerates.framerates[i],&f_rate);
            if (i) cout << ", ";
            cout << f_rate;
        }
        cout << endl;
    }
    if(params.framerate > 0) // User wants a specific frame-rate?
    {
        for(unsigned int i=0; i<framerates.num; i++){
            float f_rate;
            dc1394_framerate_as_float(framerates.framerates[i],&f_rate);
            if(fabsf(f_rate - params.framerate) <= 1.0f){
                nChosenFramerate = framerates.framerates[i];
                break;
            }
        }
    }
    float best_frame_rate=0;
    if (nChosenFramerate == DC1394_FRAMERATE_MIN) {
        // Just pick the highest frame-rate the camera can do.
        for(unsigned int i=0; i<framerates.num; i++){
            float f_rate;
            dc1394_framerate_as_float(framerates.framerates[i],&f_rate);
            if(f_rate > best_frame_rate) {
                nChosenFramerate = framerates.framerates[i];
                best_frame_rate = f_rate;
            }
        }
    }

    // Selected mode and frame-rate; Now tell the camera to use these.
    // At the moment, hard-code the channel to speed 400. This is maybe something to fix in future?
    bool speed800=false;
    if (params.bus_speed <0 || params.bus_speed == 800) {
        error=dc1394_video_set_operation_mode(camera, DC1394_OPERATION_MODE_1394B);
        if (!error)
            speed800 = (error=dc1394_video_set_iso_speed(camera, DC1394_ISO_SPEED_800))==0;
    }
    if (!speed800) {
        error = dc1394_video_set_iso_speed(camera, DC1394_ISO_SPEED_400);
    }
    if(error) { cerr << "Could not set ISO speed." << endl; return false; }

    //error = dc1394_video_set_iso_channel(camera, nCamNumber);
    //if(error) { cerr << "Could not set ISO channel." << endl; return false; }

    error = dc1394_video_set_mode(camera, nMode);
    if(error) { cerr << "Could not set video mode" << endl; return false; }

    error = dc1394_video_set_framerate(camera, nChosenFramerate);
    if(error) { cerr << "Could not set frame-rate" << endl; return false; }

    float fps;
    dc1394_framerate_as_float(nChosenFramerate,&fps);
    print_mode(cout << "DC1394: Video mode: ", camera, nMode) << " at " << fps << " fps."
            << " bus speed: " << (speed800 ? 800 : 400) << " Mbps.\n";
    dc1394_log_register_handler(DC1394_LOG_WARNING, 0, 0);

    dc1394featureset_t features;
    dc1394_feature_get_all(camera, &features);
    if (params.debug) {
	    dc1394_feature_print_all(&features, stdout);
    }
    return true;
}


#define CHECK_ERROR(err, clean, msg) if (err!=DC1394_SUCCESS) { std::cerr << msg << std::endl; clean ; return false; }
bool DC1394::initialize()
{
	d = dc1394_new ();
	if (!d)
		return false;
	dc1394camera_list_t * list;
	dc1394error_t err=dc1394_camera_enumerate (d, &list);

	if (list->num == 0) {
		dc1394_free (d);
		return false;
	}

	camera = dc1394_camera_new (d, list->ids[0].guid);
	dc1394_camera_free_list (list);
	if (!camera) {
                dc1394_log_error("Failed to initialize camera");
		dc1394_free (d);
		return false;
	}

	/*-----------------------------------------------------------------------
	 *  setup capture
	 *-----------------------------------------------------------------------*/

        /*
        err=dc1394_video_set_iso_speed(camera, DC1394_ISO_SPEED_400);
	CHECK_ERROR(err,cleanup_and_exit(camera),"Could not set iso speed");

	err=dc1394_video_set_mode(camera, DC1394_VIDEO_MODE_640x480_RGB8);
	CHECK_ERROR(err,cleanup_and_exit(camera),"Could not set video mode\n");

	dc1394_get_image_size_from_video_mode(camera, DC1394_VIDEO_MODE_640x480_RGB8, &width, &height);

	err=dc1394_video_set_framerate(camera, DC1394_FRAMERATE_30);
	CHECK_ERROR(err,cleanup_and_exit(camera),"Could not set framerate\n");
        */

        if (!init_video_mode()) {
            cleanup_and_exit(camera);
            return false;
        }
	err=dc1394_capture_setup(camera,4, DC1394_CAPTURE_FLAGS_DEFAULT);
	CHECK_ERROR(err,cleanup_and_exit(camera),"Could not setup camera-\nmake sure that the video mode and framerate are\nsupported by your camera\n");

	/*-----------------------------------------------------------------------
	 *  have the camera start sending us data
	 *-----------------------------------------------------------------------*/
	playing = false;
	start();

	return true;
}


bool DC1394::getFrame(IplImage *dst) {

	assert(dst);

	if (playing) {

		if (frame)
			dc1394_capture_enqueue(camera, frame);
		dc1394error_t err=dc1394_capture_dequeue(camera, DC1394_CAPTURE_POLICY_WAIT, &frame);
		if (err != DC1394_SUCCESS) return false;

	}
	CvMat mat;

	cvInitMatHeader(&mat, height, width, (channels == 1 ? CV_8UC1 : CV_8UC3), frame->image);

	if (((unsigned)dst->width == width) 
                        && ((unsigned)dst->height==height)
			&& (dst->nChannels == channels)) 
	{
		cvCopy(&mat, dst);
		if (channels == 3) {
			dst->channelSeq[0] = 'R';
			dst->channelSeq[1] = 'G';
			dst->channelSeq[2] = 'B';
		}
	} else if (dst->nChannels == channels) {
		cvResize(&mat, dst);
		if (channels == 3) {
			dst->channelSeq[0] = 'R';
			dst->channelSeq[1] = 'G';
			dst->channelSeq[2] = 'B';
		}
	} else if (((unsigned)dst->width == width) 
                        && ((unsigned)dst->height==height)
			&& (dst->nChannels == 1) && (channels == 3))
	{
		cvCvtColor(&mat, dst, CV_RGB2GRAY);
	} else {
		 std::cout << "DC1394: image format conversion not implemented !\n";
		 std::cout << "Trying " << dst->width << "x"<<dst->height << "x"<<dst->nChannels
			<< " against " << width << "x" << height << "x" << channels << std::endl;
	}

	return true;
}


void DC1394::getSize(int &w, int &h)
{
	w = width;
	h = height;
}

void DC1394::start()
{
	if (playing) return;

	if (dc1394_video_set_transmission(camera, DC1394_ON)!=DC1394_SUCCESS)
	{
	//	if (params.debug)
			fprintf( stderr, "unable to start camera iso transmission\n");
	}
	
	playing = true;
}

void DC1394::stop()
{
	if (playing == false) return;
	dc1394_video_set_transmission(camera, DC1394_OFF);
	playing = false;
}



