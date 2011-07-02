#ifndef __TERN_LANDMARK_TRACE_H
#define __TERN_LANDMARK_TRACE_H

#include "llvm/Pass.h"
using namespace llvm;

#include <vector>
#include <map>
using namespace std;

namespace slicer {

	struct LandmarkTrace: public ModulePass {

		static char ID;

		LandmarkTrace(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;

		const vector<unsigned> &get_thr_trunks(int thr_id) const;
		// Returns the index in the full trace. 
		// Need TraceManager to get more information. 
		unsigned get_landmark(int thr_id, size_t trunk_id) const;
		size_t get_n_trunks(int thr_id) const;
		vector<int> get_thr_ids() const;

		/* Some computation involved */
		/* 
		 * Returns whether trunk (i1, j1) happens before trunk (i2, j2).
		 * Just checking the timestamps is not enough. Only the order of
		 * enforcing landmarks will be enforced. 
		 */
		bool happens_before(int i1, size_t j1, int i2, size_t j2) const;
		/**
		 * Returns all trunks that are concurrent with the given trunk. 
		 */
		void get_concurrent_trunks(
				const pair<int, size_t> &the_trunk,
				vector<pair<int, size_t> > &concurrent_trunks) const;
		/* 
		 * Returns the latest trunk in Thread <tid2> that must happen before
		 * Trunk <trunk_id> in Thread <tid>. 
		 */
		size_t get_latest_happens_before(int tid, size_t trunk_id, int tid2) const;
		/**
		 * Used in <get_concurrent_trunk>. 
		 * Recall that there are two types of landmarks:
		 * enforcing landmarks and derived landmarks. 
		 * The order of the derived landmarks are not enforced. 
		 * i.e. Even if Trunk i happens before Trunk j in the log, Trunk i may
		 * not always happen before Trunk j if Trunk i does not end with an
		 * enforcing landmark or Trunk j does not start with an enforcing landmark.
		 * This function tries to extend the trunk region [s, e] forward and
		 * backward until the region is bounded by enforcing landmarks. 
		 * 
		 * When calling this function, <s> and <e> indicate the original region. 
		 * After it returns, <s> and <e> will indicate the extended region. 
		 */
		void extend_until_enforce(int thr_id, size_t &s, size_t &e) const;

	private:
		map<int, vector<unsigned> > thread_trunks;
	};

}

#endif
