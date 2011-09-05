#ifndef __SLICER_LANDMARK_TRACE_BUILDER_H
#define __SLICER_LANDMARK_TRACE_BUILDER_H

#include "llvm/Pass.h"
#include "common/util.h"
using namespace llvm;

#include <vector>
#include <map>
using namespace std;

namespace slicer {

	struct LandmarkTraceBuilder: public ModulePass {

		static char ID;

		LandmarkTraceBuilder(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
	};
}

#endif
