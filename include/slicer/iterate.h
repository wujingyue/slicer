#ifndef __SLICER_ITERATE_H
#define __SLICER_ITERATE_H

#include "llvm/Pass.h"
using namespace llvm;

namespace slicer {
	struct Iterate: public ModulePass {
		static char ID;

		Iterate(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
	};
}

#endif
