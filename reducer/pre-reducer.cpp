/**
 * Author: Jingyue
 */

#include "llvm/Analysis/LoopInfo.h"
using namespace llvm;

#include "pre-reducer.h"
#include "../max-slicing/clone-info-manager.h"
using namespace slicer;

static RegisterPass<PreReducer> X(
		"pre-reduce",
		"A reducer running before the integer constraint solver",
		false,
		false);

char PreReducer::ID = 0;

void PreReducer::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesCFG(); // Are you sure? 
	AU.addRequired<CloneInfoManager>();
	AU.addRequired<LoopInfo>();
	ModulePass::getAnalysisUsage(AU);
}

bool PreReducer::runOnModule(Module &M) {
	return false;
}
