#ifndef __SLICER_IDENTIFY_LOOPS_H
#define __SLICER_IDENTIFY_LOOPS_H

#include "llvm/Pass.h"
using namespace llvm;

namespace slicer {

	struct IdentifyLoops: public FunctionPass {

		static char ID;

		IdentifyLoops(): FunctionPass(&ID) {}
		virtual bool runOnFunction(Function &F);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
	};
}

#endif
