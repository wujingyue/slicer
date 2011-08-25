/**
 * Author: Jingyue
 */

#include "llvm/Support/Debug.h"
using namespace llvm;

#include "aggressive-licm.h"
using namespace slicer;

static RegisterPass<AggressiveLICM> X(
		"aggressive-licm",
		"Aggressive Loop Invariant Code Motion");

char AggressiveLICM::ID = 0;

/** Requires LoopSimplify */
void AggressiveLICM::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.addRequired<LoopInfo>();
	LoopPass::getAnalysisUsage(AU);
}

bool AggressiveLICM::runOnLoop(Loop *L, LPPassManager &LPM) {
	return false;
}
