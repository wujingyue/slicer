/**
 * Author: Jingyue
 */

#ifndef __SLICER_CONSTANTIZE_H
#define __SLICER_CONSTANTIZE_h

#include "llvm/Pass.h"
using namespace llvm;

namespace slicer {

	struct Constantize: public ModulePass {

		static char ID;

		Constantize(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
	};
}

#endif
