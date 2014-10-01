/**
 * Author: Jingyue
 */

#include "llvm/Transforms/Utils/UnrollLoop.h"
#include "llvm/Support/Debug.h"
using namespace llvm;

#include "slicer/aggressive-loop-unroll.h"
using namespace slicer;

char AggressiveLoopUnroll::ID = 0;

/* Requires LCSSA. */
void AggressiveLoopUnroll::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.addRequired<DominatorTree>();
	AU.addRequired<LoopInfo>();
}

AggressiveLoopUnroll::AggressiveLoopUnroll(): LoopPass(ID) {
}

bool AggressiveLoopUnroll::runOnLoop(Loop *L, LPPassManager &LPM) {
  return false;
#if 0
	// LoopPass is a FunctionPass. Needn't specify the function. 
	LoopInfo &LI = getAnalysis<LoopInfo>();
	DominatorTree &DT = getAnalysis<DominatorTree>();

	if (!L->isLCSSAForm(DT)) {
		errs() << "not LCSSA:" << *(L->getHeader()) << "\n";
	}
	assert(L->isLCSSAForm(DT));

	unsigned trip_count = L->getSmallConstantTripCount();
	if (trip_count == 0)
		return false;
	if (trip_count > 10)
		return false;

	if (!UnrollLoop(L, trip_count, &LI, &LPM))
		return false;

	return true;
#endif
}
