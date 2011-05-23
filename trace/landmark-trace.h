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

	private:
		map<int, vector<unsigned> > thread_trunks;
	};

}

#endif
