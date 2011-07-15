#ifndef __SLICER_REGION_MANAGER_H
#define __SLICER_REGION_MANAGER_H

namespace slicer {

	struct Region {
		int thr_id;
		size_t prev_enforcing_landmark;
		size_t next_enforcing_landmark;
	};
}

#endif
