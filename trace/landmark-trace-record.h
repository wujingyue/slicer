#ifndef __SLICER_LANDMARK_TRACE_RECORD_H
#define __SLICER_LANDMARK_TRACE_RECORD_H

namespace slicer {

	struct LandmarkTraceRecord {
		unsigned idx; // Timestamp
		unsigned ins_id;
		bool enforcing;
		int tid;
		int child_tid;
	};
}

#endif
