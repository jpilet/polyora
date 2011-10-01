//! \ingroup videosource
/*!@{*/
#ifndef __MPLAYERVIDEOSOURCE_H
#define __MPLAYERVIDEOSOURCE_H

#include <stdio.h>
#include "videosource.h"
#include "bidirpipe.h"

#define BIDIR_PIPE

/*!
 * Calls 'mencoder' to decode movies. Any thing that MPlayer can display
 * shoud be handled by this class.
 */
class MPlayerVideoSource : public VideoSource {
public:

	MPlayerVideoSource(char *input_string, char *mencoder, char *options);
	virtual bool initialize();
	virtual bool getFrame(IplImage *dst);
	void getSize(int &width, int &height);
	virtual ~MPlayerVideoSource();
	virtual void start();
	virtual void stop();
	virtual bool isPlaying() { return playing; };
	virtual const char *getStreamName() const { return input_string; }
	virtual const char *getStreamType() const { return "MPlayerVideoSource"; }
	virtual int getId();

private:

	void readInfo();
	bool execPlayer();
	void closePlayer();

	char *input_string;
	char *mencoder;
	char *mencoder_options;
	int frameCnt;
	bool playing;
	char pipe_dir[64];
	char video_pipe[64];
	char mencoder_cmd[4096];
	IplImage *tmp_im;
	int width, height;

#ifdef BIDIR_PIPE
	bidir_pipe pipe;
#endif
	FILE *info_pipe;
	int data_pipe;
};

class MPlayerFactory : public ParticularVSFactory {
public:
	MPlayerFactory() { name="MPlayer"; input_string=0; mencoder=0; };
	virtual void registerParameters(ParamSection *sec);
	virtual VideoSource *construct();
private:
	char *input_string;
	char *mencoder;
	char *mencoder_options;
	bool use;
};

#endif
/*!@}*/
