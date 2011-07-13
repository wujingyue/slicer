/**
 * Author: Jingyue
 */

#include "llvm/Transforms/Utils/UnrollLoop.h"
using namespace llvm;

#include "aggressive-loop-unroll.h"
using namespace slicer;

static RegisterPass<AggressiveLoopUnroll> X(
		"aggressive-loop-unroll",
		"Aggressively unroll loops even if it contains function calls");

char AggressiveLoopUnroll::ID = 0;

/* Requires LCSSA. */
void AggressiveLoopUnroll::getAnalysisUsage(AnalysisUsage &AU) const {
#if 0
	AU.addRequiredID(LCSSAID);
#endif
	AU.addRequired<LoopInfo>();
#if 0
	AU.addPreservedID(LCSSAID);
#endif
	LoopPass::getAnalysisUsage(AU);
}

bool AggressiveLoopUnroll::runOnLoop(Loop *L, LPPassManager &LPM) {

	LoopInfo &LI = getAnalysis<LoopInfo>();
	assert(L->isLCSSAForm());

	unsigned trip_count = L->getSmallConstantTripCount();
	if (trip_count == 0)
		return false;

	if (!UnrollLoop(L, trip_count, &LI, &LPM))
		return false;

	return true;
}
