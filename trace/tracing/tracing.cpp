#include <iostream>
#include <fstream>
using namespace std;

#include "../trace.h"
using namespace slicer;

#include "pthread.h"

void append_to_trace(const TraceRecord &record) {
	ofstream fout("/tmp/fulltrace", ios::binary | ios::app);
	fout.write((char *)&record, sizeof record);
}

/*
 * Injected to the traced program
 */
extern "C" void trace_inst(unsigned ins_id) {
	TraceRecord record;
	record.ins_id = ins_id;
	record.raw_tid = pthread_self();
	record.raw_child_tid = INVALID_RAW_TID;
	append_to_trace(record);
}

extern "C" int trace_pthread_create(
		unsigned ins_id, pthread_t *thread, const pthread_attr_t *attr,
		void *(*start_routine)(void *), void *arg) {
	TraceRecord record;
	record.ins_id = ins_id;
	record.raw_tid = pthread_self();
	int ret = pthread_create(thread, attr, start_routine, arg);
	record.raw_child_tid = *thread;
	append_to_trace(record);
	return ret;
}
