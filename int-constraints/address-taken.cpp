/**
 * Author: Jingyue
 *
 * Collect integer constraints on address-taken variables. 
 */
#include "config.h"
#include "capture.h"

namespace slicer {

	/*
	 * Algorithm:
	 * 1. Pre-compute all must-aliasing sets. 
	 * 2. For each must-aliasing set, compute the trunks that are blocked by
	 * the set. 
	 * 3. For each load from (int *), compute all the stores whose values may
	 * flow to this load. 
	 */
	void CaptureConstraints::capture_addr_taken_vars(Module &M) {
		
	}
}
