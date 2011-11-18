/**
 * Author: Jingyue
 */

#include "llvm/Transforms/Utils/UnrollLoop.h"
#include "llvm/Support/Debug.h"
#include "slicer/InitializePasses.h"
using namespace llvm;

#include "slicer/aggressive-loop-unroll.h"
using namespace slicer;

INITIALIZE_PASS_BEGIN(AggressiveLoopUnroll, "aggressive-loop-unroll",
		"Aggressively unroll loops even if it contains function calls",
		false, false)
INITIALIZE_PASS_DEPENDENCY(LoopInfo)
INITIALIZE_PASS_DEPENDENCY(DominatorTree)
INITIALIZE_PASS_END(AggressiveLoopUnroll, "aggressive-loop-unroll",
		"Aggressively unroll loops even if it contains function calls",
		false, false)

char AggressiveLoopUnroll::ID = 0;

/* Requires LCSSA. */
void AggressiveLoopUnroll::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.addRequired<LoopInfo>();
	AU.addRequired<DominatorTree>();
}

AggressiveLoopUnroll::AggressiveLoopUnroll(): LoopPass(ID) {
	initializeAggressiveLoopUnrollPass(*PassRegistry::getPassRegistry());
}

bool AggressiveLoopUnroll::runOnLoop(Loop *L, LPPassManager &LPM) {
#if 0
	dbgs() << "AggressiveLoopUnroll::runOnLoop: " <<
		L->getHeader()->getParent()->getName() << "." <<
		L->getHeader()->getName() << "\n";
#endif

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
#if 1
	if (trip_count > 10)
		return false;
#endif

	if (!UnrollLoop(L, trip_count, &LI, &LPM))
		return false;

	return true;
}
