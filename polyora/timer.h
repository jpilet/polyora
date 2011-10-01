/*  This file is part of Polyora, a multi-target tracking library.
    Copyright (C) 2010 Julien Pilet

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program. If not, see <http://www.gnu.org/licenses/>.

    To contact the author of this program, please send an e-mail to:
    julien.pilet(at)calodox.org
*/
#ifndef _TIMER_H
#define _TIMER_H

#include <map>
#include <list>
#include <string.h>

#ifdef WIN32
#include <windows.h>
typedef LARGE_INTEGER time_type;
#else
#include <sys/time.h>
typedef struct timeval time_type;
#endif

time_type get_time();
double time_to_msec(time_type &t);
time_type operator-(const time_type &a, const time_type &b);

struct cmp_str
{
   bool operator()(char const *a, char const *b)
   {
      return strcmp(a, b) < 0;
   }
};


class Timer {
public:
	Timer() { start(); }
	void start();
	double stop();
	void resume();
	double value(); 

	bool isrunning() const { return running; }
private:
	bool running;
	double duration();

	

	time_type s;



	double total;
};

class TaskTimer : public Timer {
public:

	TaskTimer(const char *name);
	TaskTimer *setTask(const char *task);
	double stop();

	static void pushTask(const char *task);
	static void popTask();
	static void printStats();
	
protected:
	typedef std::list<TaskTimer> TaskList;

	struct SelfIncl {
		double self, incl;
		SelfIncl() : self(0), incl(0) {}
	} ;
	typedef std::map<const char *, SelfIncl> CharDoubleMap;

	void printStats(int indent, double total, CharDoubleMap &flatProfile);

	const char *name;
	//typedef std::map<const char *, TaskTimer> TaskMap;
	//TaskMap subtasks;
	TaskList subtasks;
	TaskList::iterator currentTask;

#ifdef _OPENMP
	struct ThreadedTask;
	struct ThreadedTask {
		int nb_thread;
		double tot_time,self;

		typedef std::map<const char *, ThreadedTask, cmp_str> SubTaskMap;
		SubTaskMap subtasks;

		ThreadedTask() { nb_thread=0; tot_time=0; }

		void print(const char *name, int indent, double total);
		void cmpFlatProfile(const char *name, CharDoubleMap &flatProfile);
	};

	void addThreadTask(ThreadedTask &task);
	static double printThreadTreeInfo(CharDoubleMap &flatProfile);
#endif
};



#endif
