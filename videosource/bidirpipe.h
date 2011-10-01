//! \ingroup videosource
/*!@{*/
#ifndef BIDIR_PIPE_H
#define BIDIR_PIPE_H

#include	<stdio.h>
#include <unistd.h>

struct bidir_pipe {
	FILE *read;
	FILE *write;
	pid_t pid;

	bidir_pipe();
	bool open(const char *command, const char *mode);
	int close(bool kill_process);
	~bidir_pipe() { close(false); }
};

#endif
/*!@}*/
