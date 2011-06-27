#ifndef __SLICER_PASS_B_H
#define __SLICER_PASS_B_H

#include "llvm/Pass.h"
using namespace llvm;

namespace slicer {

	struct PassB: public ModulePass {

		static char ID;

		PassB();
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void releaseMemory();
	};
}

#endif
