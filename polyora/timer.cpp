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
#include <map>
#include <stdio.h>
#include "timer.h"
#include <string.h>
#include <assert.h>

typedef std::list<TaskTimer *> TaskStack;

#ifdef _OPENMP
#include <omp.h>

#define MAX_THREADS 256
static TaskStack thread_stacks[MAX_THREADS];

#else
static TaskStack stack;
#endif

#ifdef WIN32
time_type get_time(){
	time_type r;
	QueryPerformanceCounter(&r);
	return r;
}

double time_to_msec(time_type &t) {
	time_type freq;
	 QueryPerformanceFrequency(&freq);
	 return (double)(t.QuadPart) / (double)freq.QuadPart;
}

time_type operator-(const time_type &a, const time_type &b) {
	time_type r;
	r.QuadPart = a.QuadPart - b.QuadPart;
	return r;
}
#else
time_type get_time(){
	time_type r;
	gettimeofday(&r,0);
	return r;
}

double time_to_msec(time_type &t) {
	return t.tv_sec * 1000.0 + t.tv_usec/1000.0;
}

time_type operator-(const time_type &a, const time_type &b) {
	time_type r;
	timersub(&a,&b, &r);
	return r;
}
#endif


void Timer::start() {
	total=0;
	running=true;
	s = get_time();
}

double Timer::stop() {
	double r = value();
	running = false;
	return r;
}

double Timer::value() {
	if (running)
		total += duration();
	return total;
}


void Timer::resume() {
	if (!running) {
		s = get_time();
		running = true;
	}
}


double Timer::duration() {
	time_type end, dt;
	end = get_time();
	dt = end - s;
	s = end;
	return time_to_msec(dt);
}

TaskTimer::TaskTimer(const char *name) : name(name), currentTask(subtasks.end()) {
}

#ifdef ENABLE_PROFILE
double TaskTimer::stop() {
	if (subtasks.size()>0 && currentTask != subtasks.end()) currentTask->stop();
	return Timer::stop();
}

TaskTimer *TaskTimer::setTask(const char *task) {

	if (subtasks.size()>0 && currentTask != subtasks.end()) {
		currentTask->stop();
	}
	currentTask=subtasks.end();
	if (!task) return 0;

	for (TaskList::iterator it = subtasks.begin(); it!=subtasks.end(); ++it) {
		if (strcmp(task, it->name)==0) {
			currentTask = it;
			break;
		}
	}
	if (currentTask != subtasks.end()) {
		currentTask->resume();
		return &(*currentTask);
	} else {
		TaskTimer t(task);
		subtasks.push_front(t);
		currentTask = subtasks.begin();
		assert(currentTask->name == task);
		return &(*currentTask);
	}
}

#ifdef _OPENMP

void TaskTimer::addThreadTask(ThreadedTask &task)
{
	task.tot_time += value();
	task.nb_thread++;

	double sub = 0;
	for (TaskList::iterator it = subtasks.begin(); it!=subtasks.end(); ++it) {
		it->addThreadTask(task.subtasks[it->name]);
		sub += it->value();
	}
	task.self += value() - sub;
}
void TaskTimer::ThreadedTask::cmpFlatProfile(const char *name, CharDoubleMap &flatProfile)
{
	flatProfile[name].incl += tot_time;
	flatProfile[name].self += self;

	for (SubTaskMap::iterator it = subtasks.begin();
			it!=subtasks.end();
			++it) {
		it->second.cmpFlatProfile(it->first, flatProfile);
	}
}

void TaskTimer::ThreadedTask::print(const char *name, int indent, double total)
{

	printf("% 10.1fms % 7.2f%%  %10.1fms % 7.2f%%", tot_time, 100.0*tot_time/total, 
			self, 100.0f*self/total);

	for (int i=1; i<indent; i++)
		printf(" | ");
	if (indent>=1)
		printf (" +-");
	printf(" %s\n", name);

	for (SubTaskMap::iterator it = subtasks.begin();
			it!=subtasks.end();
			++it)
	{
		it->second.print(it->first, indent+1,total);
	}
}

double TaskTimer::printThreadTreeInfo(CharDoubleMap &flatProfile)
{
	ThreadedTask tasks;

	for (int i=0; i< MAX_THREADS; i++) {
		if (thread_stacks[i].size() > 0) {
			thread_stacks[i].front()->addThreadTask(tasks);

		}
	}
	tasks.cmpFlatProfile("Root", flatProfile);
	tasks.print("Root", 0, tasks.tot_time);
	return tasks.tot_time;
}

#endif

void TaskTimer::printStats(int indent, double total, CharDoubleMap &flatProfile) {
	
	double incl = value();

	double self= incl;
	for (TaskList::iterator it = subtasks.begin(); it!=subtasks.end(); ++it) {
		self -= it->value();
	}
	printf("% 10.1fms % 7.2f%%  %10.1fms % 7.2f%%", incl, 100.0*incl/total, 
			self, 100.0f*self/total);

	flatProfile[name].self += self;
	flatProfile[name].incl += incl;

	for (int i=1; i<indent; i++)
		printf(" | ");
	if (indent>=1)
		printf (" +-");
	printf(" %s\n", name);

	for (TaskList::iterator it = subtasks.begin(); it!=subtasks.end(); ++it) {
		it->printStats(indent+1, total, flatProfile);
	}
}

void TaskTimer::pushTask(const char *task) {
#ifdef _OPENMP
	TaskStack &stack = thread_stacks[omp_get_thread_num()];
#endif
	if (stack.size() == 0) 
		stack.push_back(new TaskTimer("Other/Idle"));

	stack.push_back(stack.back()->setTask(task));
}

void TaskTimer::popTask()
{
#ifdef _OPENMP
	TaskStack &stack = thread_stacks[omp_get_thread_num()];
#endif
	// it is forbidden to pop Root
	assert(stack.size()>1);
	stack.back()->stop();
	stack.pop_back();
	stack.back()->resume();
}

void TaskTimer::printStats() 
{
	CharDoubleMap flatProfile;

	double total=0;

	printf("Tree Profile:\n   Incl-Abs   Incl-%%     Self-Abs   Self-%%      Name\n"); 

#ifdef _OPENMP
	total = printThreadTreeInfo(flatProfile);
#else
	TaskTimer *root = (stack.size()>0 ? stack.front() : 0);

	if (root) {
		total += root->value();
		root->printStats(0, root->value(), flatProfile);
	}
#endif
	
	printf("Flat Profile:\n   Incl-Abs   Incl-%%     Self-Abs   Self-%%      Name\n"); 
	for (CharDoubleMap::iterator it=flatProfile.begin(); it!=flatProfile.end(); ++it) {
		printf("% 10.1fms % 7.2f%%  %10.1fms % 7.2f%% %s\n", 
				it->second.incl, 100.0f*it->second.incl/total, 
				it->second.self, 100.0f*it->second.self/total,
				it->first);
	}
}
#else
double TaskTimer::stop() { return 0; }
TaskTimer *TaskTimer::setTask(const char *task) {return 0;}
void TaskTimer::printStats(int indent, double total, CharDoubleMap &flatProfile) {}
void TaskTimer::pushTask(const char *task) {}
void TaskTimer::popTask() {}
void TaskTimer::printStats() {}
#endif
