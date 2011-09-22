/**
 * Author: Jingyue
 */

#include "llvm/Transforms/Utils/UnrollLoop.h"
#include "llvm/Support/Debug.h"
using namespace llvm;

#include "slicer/aggressive-loop-unroll.h"
using namespace slicer;

static RegisterPass<AggressiveLoopUnroll> X("aggressive-loop-unroll",
		"Aggressively unroll loops even if it contains function calls");

char AggressiveLoopUnroll::ID = 0;

/* Requires LCSSA. */
void AggressiveLoopUnroll::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.addRequired<LoopInfo>();
	LoopPass::getAnalysisUsage(AU);
}

bool AggressiveLoopUnroll::runOnLoop(Loop *L, LPPassManager &LPM) {
#if 0
	dbgs() << "AggressiveLoopUnroll::runOnLoop: " <<
		L->getHeader()->getParent()->getName() << "." <<
		L->getHeader()->getName() << "\n";
#endif

	LoopInfo &LI = getAnalysis<LoopInfo>();

	if (!L->isLCSSAForm()) {
		errs() << "not LCSSA:" << *(L->getHeader()) << "\n";
	}
	assert(L->isLCSSAForm());

	unsigned trip_count = L->getSmallConstantTripCount();
	if (trip_count == 0)
		return false;
#if 1
	if (trip_count > 10)
		return false;
#endif

	if (!UnrollLoop(L, trip_count, &LI, &LPM))
		return false;

	return true;
}
