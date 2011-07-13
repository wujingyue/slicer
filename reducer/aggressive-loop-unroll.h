#ifndef __SLICER_AGGRESSIVE_LOOP_UNROLL_H
#define __SLICER_AGGRESSIVE_LOOP_UNROLL_H

#include "llvm/Pass.h"
#include "llvm/Analysis/LoopPass.h"
using namespace llvm;

namespace slicer {
	
	struct AggressiveLoopUnroll: public LoopPass {

		static char ID;

		AggressiveLoopUnroll(): LoopPass(&ID) {}
		virtual bool runOnLoop(Loop *L, LPPassManager &LPM);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
	};
}

#endif
