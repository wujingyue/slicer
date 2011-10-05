/**
 * Author: Jingyue
 */

#ifndef __SLICER_AGGRESSIVE_LICM_H
#define __SLICER_AGGRESSIVE_LICM_H

#include "llvm/Pass.h"
#include "llvm/Analysis/LoopPass.h"
using namespace llvm;

namespace slicer {
	struct AggressiveLICM: public LoopPass {
		static char ID;

		AggressiveLICM(): LoopPass(&ID) {}
		virtual bool runOnLoop(Loop *L, LPPassManager &LPM);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
	};
}

#endif
