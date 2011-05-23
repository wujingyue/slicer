#include <iostream>
#include <fstream>
#include <errno.h>
using namespace std;

#include "../trace.h"
using namespace slicer;

#include "pthread.h"

static pthread_mutex_t trace_mutex = PTHREAD_MUTEX_INITIALIZER;

static void append_to_trace(const TraceRecord &record) {
	pthread_mutex_lock(&trace_mutex);
	ofstream fout("/tmp/fulltrace", ios::binary | ios::app);
	fout.write((char *)&record, sizeof record);
	pthread_mutex_unlock(&trace_mutex);
}

/*
 * Injected to the traced program
 * Need restore <errno> at the end. 
 */
extern "C" void trace_inst(unsigned ins_id) {
	int saved_errno = errno;
	TraceRecord record;
	record.ins_id = ins_id;
	record.raw_tid = pthread_self();
	record.raw_child_tid = INVALID_RAW_TID;
	append_to_trace(record);
	errno = saved_errno;
}

/* The wrapper to pthread_create */
/* Restore <errno> to be the one right after <pthread_create>. */
extern "C" int trace_pthread_create(
		unsigned ins_id, pthread_t *thread, const pthread_attr_t *attr,
		void *(*start_routine)(void *), void *arg) {
	TraceRecord record;
	record.ins_id = ins_id;
	record.raw_tid = pthread_self();
	int ret = pthread_create(thread, attr, start_routine, arg);
	int saved_errno = errno;
	record.raw_child_tid = *thread;
	append_to_trace(record);
	errno = saved_errno;
	return ret;
}
