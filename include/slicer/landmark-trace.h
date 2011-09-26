/**
 * Author: Jingyue
 *
 * TODO: Renmae to LandmarkTraceManager. 
 * TODO: Introduce the concept of region. Move "struct region" definition
 * here. 
 */

#ifndef __SLICER_LANDMARK_TRACE_H
#define __SLICER_LANDMARK_TRACE_H

#include "llvm/Pass.h"
#include "common/util.h"
using namespace llvm;

#include <vector>
#include <map>
using namespace std;

#include "landmark-trace-record.h"
using namespace slicer;

namespace slicer {

	struct LandmarkTrace: public ModulePass {

		static char ID;

		LandmarkTrace(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;

		// Returns the index in the full trace. 
		unsigned get_landmark_timestamp(int thr_id, size_t trunk_id) const;
		const LandmarkTraceRecord &get_landmark(int thr_id, size_t trunk_id) const;
		bool is_enforcing_landmark(int thr_id, size_t trunk_id) const;
		size_t get_n_trunks(int thr_id) const;
		vector<int> get_thr_ids() const;
		const vector<LandmarkTraceRecord> &get_thr_trunks(int thr_id) const;

		/* Some computation involved */
		/* 
		 * Returns whether trunk (i1, j1) happens before trunk (i2, j2).
		 * Just checking the timestamps is not enough. Only the order of
		 * enforcing landmarks will be enforced. 
		 */
		bool happens_before(int i1, size_t j1, int i2, size_t j2) const;
		/**
		 * Returns all trunks that are concurrent with the given trunk. 
		 * We guarantee the output list doesn't contain duplicated items. 
		 */
		void get_concurrent_trunks(
				const pair<int, size_t> &the_trunk,
				vector<pair<int, size_t> > &concurrent_trunks) const;
		/**
		 * A region is a contiguous set of trunks. 
		 * We guarantee the output regions don't overlap with each other. 
		 */
		void get_concurrent_regions(
				const pair<int, size_t> &the_trunk,
				vector<pair<int, pair<size_t, size_t> > > &concurrent_regions) const;
		/* 
		 * Returns the latest landmark in Thread <tid2> that must happen before
		 * Trunk <trunk_id> in Thread <tid>. 
		 */
		size_t get_latest_happens_before(int tid, size_t trunk_id, int tid2) const;
		/* Find the first landmark in Thread <thr_id> whose landmark >= <idx> */
		size_t search_landmark_in_thread(int thr_id, unsigned idx) const;

	private:
		/**
		 * Recall that there are two types of landmarks:
		 * enforcing landmarks and derived landmarks. 
		 * The order of the derived landmarks are not enforced. 
		 * i.e. Even if Trunk i happens before Trunk j in the log, Trunk i may
		 * not always happen before Trunk j if Trunk i does not end with an
		 * enforcing landmark or Trunk j does not start with an enforcing landmark.
		 * This function tries to extend the trunk region [s, e] forward and
		 * backward until the region is bounded by enforcing landmarks or the
		 * starts/ends of a thread function. 
		 * 
		 * When calling this function, <s> and <e> indicate the original region. 
		 * After it returns, <s> and <e> will indicate the extended region. 
		 */
		void extend_until_enforce(int thr_id, size_t &s, size_t &e) const;
		map<int, vector<LandmarkTraceRecord> > thread_trunks;
	};

}

#endif
