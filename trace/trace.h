#ifndef __SLICER_TRACE_H
#define __SLICER_TRACE_H

namespace slicer {

	const static unsigned long INVALID_RAW_TID = -1;

	// Directly collected from executing the instrumented program
	struct TraceRecord {
		unsigned ins_id;
		unsigned long raw_tid;
		unsigned long raw_child_tid;
	};

}

#endif
