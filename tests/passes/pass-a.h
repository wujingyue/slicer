#ifndef __SLICER_PASS_A_H
#define __SLICER_PASS_A_H

#include "llvm/Pass.h"
using namespace llvm;

namespace slicer {

	struct PassA: public ModulePass {

		static char ID;

		PassA();
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void releaseMemory();
		int getSize() const { return size; }

	private:
		int size;
	};
}

#endif
