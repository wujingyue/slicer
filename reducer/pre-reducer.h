/**
 * Author: Jingyue
 */

#ifndef __SLICER_PRE_REDUCER_H
#define __SLICER_PRE_REDUCER_H

#include "llvm/Pass.h"
using namespace llvm;

namespace slicer {

	struct PreReducer: public ModulePass {

		static char ID;

		PreReducer(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
	};
}

#endif
