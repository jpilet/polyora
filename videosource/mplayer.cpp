
// Includes for the many system calls
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include <iostream>

#include "mplayer.h"
#include "iniparser.h"

using namespace std;

void MPlayerFactory::registerParameters(ParamSection *sec) {
	sec->addStringParam("mplayer.input", &input_string, "movie.avi");
	sec->addBoolParam("mplayer.use", &use, true);
	sec->addStringParam("mplayer.mencoder", &mencoder, "mencoder");
	sec->addStringParam("mplayer.options", &mencoder_options, "-ovc raw -of rawvideo -vf format=bgr24 -quiet");
};

VideoSource *MPlayerFactory::construct() {
	if (use) {
		MPlayerVideoSource *vs = new MPlayerVideoSource(input_string, mencoder, mencoder_options);
		if (vs->initialize()) return vs;
		delete vs;
	}
	return 0;
}

MPlayerVideoSource::MPlayerVideoSource(char *input_string, char *mencoder, char *options)
	: input_string(input_string), mencoder(mencoder), mencoder_options(options)
{	
	frameCnt=0;
	playing=true;
	pipe_dir[0]=0;
	video_pipe[0]=0;
	info_pipe=0;
	data_pipe=-1;
	tmp_im=0;
}


bool MPlayerVideoSource::initialize() 
{

	sprintf(pipe_dir, "/tmp/vsmencXXXXXX");

	if (mkdtemp(pipe_dir)==0) {
		perror(pipe_dir);
		return false;
	}

	snprintf(video_pipe, sizeof(video_pipe), "%s/pipe", pipe_dir);

	if (mknod(video_pipe, S_IFIFO | S_IREAD | S_IWRITE, 0) <0) {
		perror("Named pipe");
		rmdir(pipe_dir);
		pipe_dir[0]=0;
		return false;
	}

	snprintf(mencoder_cmd, sizeof(mencoder_cmd), "%s %s %s -o %s", 
			mencoder, mencoder_options, input_string, video_pipe);

	return execPlayer();
}

static bool searchString(FILE *f, const char *s) {
	int i=0;

	while (1) {
		int c = fgetc(f);

		if (feof(f)) return false;

		fputc(c,stdout);

		if (c==s[i]) {
			i++;
			if (s[i]==0) return true;
		} else
			i=0;
	}
	return false;

}

bool MPlayerVideoSource::execPlayer() {

	assert(data_pipe<0);
	assert(info_pipe==0);

	data_pipe = open(video_pipe,O_RDONLY | O_NONBLOCK);

	if (data_pipe < 0) {
		perror(video_pipe);
		return false;
	}

	// put data_pipe into blocking mode
	fcntl(data_pipe, F_SETFL, fcntl(data_pipe, F_GETFL) & (~O_NONBLOCK));

#ifdef BIDIR_PIPE
	if (pipe.open(mencoder_cmd,"r")==0) {
		perror(mencoder_cmd);
		return(false);
	}
	info_pipe = pipe.read;
#else
	info_pipe = popen(mencoder_cmd, "r");
	if (info_pipe ==0) {
		perror(mencoder_cmd);
		return false;
	}
#endif

	if (!searchString(info_pipe, "success:")
			|| !(searchString(info_pipe, "VIDEO:")
                                && searchString(info_pipe, "] "))) {
		closePlayer();
		return false;
	}

	if (fscanf(info_pipe, "%dx%d", &width, &height) <= 0) {
		fprintf(stderr, "Error while reading mencoder output: unable to determine image resolution.");
		closePlayer();
		return false;
	}
        fprintf(stderr, "MPlayer: got resolution %dx%d\n", width, height);

	// info pipe -> into non-blocking mode
	fcntl(fileno(info_pipe), F_SETFL, fcntl(fileno(info_pipe), F_GETFL) |O_NONBLOCK);

	// transmit what mplayer says
	readInfo();

	return true;
}

void MPlayerVideoSource::closePlayer() 
{
	if (data_pipe >= 0) {
		// put data_pipe in non blocking mode
		fcntl(data_pipe, F_SETFL, fcntl(data_pipe, F_GETFL) | O_NONBLOCK);
		close(data_pipe);
	}
	data_pipe=-1;

	if (info_pipe) {
		readInfo();
		
#ifdef BIDIR_PIPE
		pipe.close(true);
#else
		close(fileno(info_pipe));
		pclose(info_pipe);
#endif
	}
	info_pipe=0;
}

void MPlayerVideoSource::readInfo() {
	char str[80];
	int n=0;

	while (!feof(info_pipe)) {
		n = fread(str, 1, sizeof(str), info_pipe);
		if (n>0) {
			fwrite(str, 1, n, stdout);
		} else
			return;	
	}
}

bool MPlayerVideoSource::getFrame(IplImage *dst) 
{
	if (playing) {
		int bytes = width*height*3;

		IplImage *read_to;
		if ((width == dst->width) && (height == dst->height)) 
			read_to = dst;
		else {
			if (tmp_im ==0) tmp_im = cvCreateImage(cvSize(width,height), IPL_DEPTH_8U,3);
			read_to = tmp_im;
		}

		int got=0;
		do {
			readInfo();
			int n = read(data_pipe, read_to->imageData+got, bytes-got);
			if (n<0) {
				printf("read() returned %d\n", n);
				playing = false;
				return false;
			}
			if (frameCnt>0 && n==0) { 
				// end of file. Restart reading.
				closePlayer();
				if (!execPlayer()) return false;
				frameCnt=0;
				got=0;
			}

			got += n;
		} while (got!= bytes);


		if (read_to == tmp_im)
			cvResize(tmp_im, dst);

		frameCnt++;
	}

	return true;
}

int MPlayerVideoSource::getId() 
{
	if (playing) return frameCnt-1;
	return frameCnt;
}

MPlayerVideoSource::~MPlayerVideoSource() {
	cout << "now closing mencoder.\n";
	closePlayer();
	if (video_pipe[0]) unlink(video_pipe);
	if (strlen(pipe_dir)>0) rmdir(pipe_dir);
	if (tmp_im) cvReleaseImage(&tmp_im);
}

void MPlayerVideoSource::start() {
	playing = true;
}

void MPlayerVideoSource::stop() {
	playing = false;
}

void MPlayerVideoSource::getSize(int &width, int &height)
{
	width = this->width;
	height = this->height;
}
