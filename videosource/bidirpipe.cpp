/*  popen.c -  a version of the Unix popen() library function
 *	FILE *popen( char *command, char *mode )
 *		command is a regular shell command
 *		mode is "r" or "w" 
 *		returns a stream attached to the command, or NULL
 *		execls "sh" "-c" command
 *    todo: what about signal handling for child process?
 */
#include "bidirpipe.h"
#include	<signal.h>
#include	<sys/wait.h>
#include	<errno.h>
#include	<fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#define	SHELL	"/bin/sh"

bidir_pipe::bidir_pipe() {
	read=write=0;
	pid=0;
}

bool bidir_pipe::open(const char *cmdstring, const char *type)
{
	printf("Executing: %s\n", cmdstring);
	int pfd[2];
	int pfd2[2];

	if (pipe(pfd) < 0)
		return(false);	/* errno set by pipe() */

	if (*type!='r' && *type != 'w') {
		if (pipe(pfd2) < 0)
			return(false);	/* errno set by pipe() */
	}

	if ( (pid = fork()) < 0)
		return(false);	/* errno set by fork() */

	else if (pid == 0) { /* child */
		if (*type == 'r') {
			::close(pfd[0]);
			if (pfd[1] != STDOUT_FILENO) {
				dup2(pfd[1], STDOUT_FILENO);
				::close(pfd[1]);
			}
		} else if (*type == 'r') {
			::close(pfd[1]);
			if (pfd[0] != STDIN_FILENO) {
				dup2(pfd[0], STDIN_FILENO);
				::close(pfd[0]);
			}
		} else {
			if (pfd[1] != STDOUT_FILENO) {
				dup2(pfd[1], STDOUT_FILENO);
				::close(pfd[1]);
			}
			if (pfd2[0] != STDIN_FILENO) {
				dup2(pfd2[0], STDIN_FILENO);
				::close(pfd2[0]);
			}
		}

		setpgid(0,0);
		execl(SHELL, "sh", "-c", cmdstring, (char *) 0);
		_exit(127);
	}

	/* parent */
	if (*type == 'r') {
		::close(pfd[1]);
		write=0;
		if ( (read = fdopen(pfd[0], type)) == NULL)
			return(false);
	} else if (*type == 'w') {
		::close(pfd[0]);
		read=0;
		if ( (write = fdopen(pfd[1], type)) == NULL)
			return(false);
	} else {
		::close(pfd[1]);
		::close(pfd2[0]);
		if ( (read = fdopen(pfd[0], "r")) == NULL)
			return(false);
		if ( (write = fdopen(pfd2[1], "w")) == NULL) {
			fclose(read);
			read=0;
			return(false);
		}
	}
	return(true);
}

int bidir_pipe::close(bool k)
{
	if (pid==0) return -1;

	if (read) fclose(read);
	read=0;
	if (write) fclose(write);
	write=0;

	if (k) {
		if (killpg(pid, SIGTERM)<0) {
			char str[64];
			sprintf(str,"killpg %d",pid);
			perror(str);
		}
	} 
	int stat;
	while (waitpid(pid, &stat, 0) < 0)
		if (errno != EINTR)
			return(-1);	/* error other than EINTR from waitpid() */
	pid=0;

	return(stat);	/* return child's termination status */
}




