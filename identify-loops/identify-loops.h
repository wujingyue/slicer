#ifndef __SLICER_IDENTIFY_LOOPS_H
#define __SLICER_IDENTIFY_LOOPS_H

#include "llvm/Pass.h"
#include "llvm/Analysis/LoopPass.h"
using namespace llvm;

namespace slicer {

	struct IdentifyLoops: public LoopPass {

		static char ID;

		IdentifyLoops(): LoopPass(&ID) {}
		virtual bool runOnLoop(Loop *L, LPPassManager &LPM);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
	};
}

#endif
