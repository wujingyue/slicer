/**
 * Author: Jingyue
 */

#define DEBUG_TYPE "reducer"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/Debug.h"
#include "rcs/util.h"
#include "rcs/FPCallGraph.h"
using namespace llvm;

#include "slicer/aggressive-promotion.h"
#include "slicer/clone-info-manager.h"
#include "slicer/landmark-trace.h"
#include "slicer/may-write-analyzer.h"
#include "slicer/adv-alias.h"
using namespace slicer;

/*
 * AggressivePromotion requires LoopSimplify to insert pre-headers.
 * But strangely, I'm not able to addRequiredID(LoopSimplifyID)
 * in this function. 
 * Therefore, the simplifier have to add LoopSimplify. 
 */
void AggressivePromotion::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesCFG(); // Are you sure? 
	AU.addRequired<DominatorTree>();
	AU.addRequired<LoopInfo>();
	AU.addRequired<LandmarkTrace>();
	AU.addRequired<CloneInfoManager>();
	AU.addRequired<FPCallGraph>();
	AU.addRequired<TargetData>();
	AU.addRequired<MayWriteAnalyzer>();
}

AggressivePromotion::AggressivePromotion(): LoopPass(ID) {
}

char AggressivePromotion::ID = 0;

bool AggressivePromotion::runOnLoop(Loop *L, LPPassManager &LPM) {
	DT = &getAnalysis<DominatorTree>();
	LI = &getAnalysis<LoopInfo>();

	assert(L->getHeader());
	BasicBlock *preheader = L->getLoopPreheader();
	if (!preheader)
		return false;

	return hoist_region(L, DT->getNode(L->getHeader()));
}

bool AggressivePromotion::hoist_region(Loop *L, DomTreeNode *node) {
	BasicBlock *bb = node->getBlock();
	if (!L->contains(bb))
		return false;

	// Skip <bb> if it is in a subloop. Already handled by previous passes. 
	if (LI->getLoopFor(bb) != L)
		return false;

	bool changed = false;
	for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ) {
		// inv->moveBefore invalidates the next pointer in <ins>. 
		BasicBlock::iterator next = ins; ++next;
		if (can_hoist(L, ins)) {
			DEBUG(dbgs() << "Hoisting" << *ins << "\n");
			ins->moveBefore(L->getLoopPreheader()->getTerminator());
			changed = true;
		}
		ins = next;
	}

	const vector<DomTreeNode *> &children = node->getChildren();
	for (unsigned i = 0; i < children.size(); ++i)
		changed |= hoist_region(L, children[i]);
	return changed;
}

bool AggressivePromotion::can_hoist(Loop *L, Instruction *ins) {
	// Cannot hoist <ins> if not all its operands are loop invariants. 
	assert(ins);
	if (!L->hasLoopInvariantOperands(ins))
		return false;

	if (!is_safe_to_execute_unconditionally(L, ins))
		return false;

	if (LoadInst *li = dyn_cast<LoadInst>(ins))
		return !loop_may_write(L, li->getPointerOperand());

	return isa<BinaryOperator>(ins) || isa<CastInst>(ins) ||
		isa<SelectInst>(ins) || isa<GetElementPtrInst>(ins) || isa<CmpInst>(ins) ||
		isa<InsertElementInst>(ins) || isa<ExtractElementInst>(ins) ||
		isa<ShuffleVectorInst>(ins);
}

bool AggressivePromotion::is_safe_to_execute_unconditionally(
		Loop *L, Instruction *ins) {
	if (isSafeToSpeculativelyExecute(ins))
		return true;

	// return true if it dominates all loop exits.
	SmallVector<BasicBlock *, 8> exits;
	L->getExitBlocks(exits);
	for (unsigned i = 0; i < exits.size(); ++i) {
		if (!DT->dominates(ins->getParent(), exits[i]))
			return false;
	}

	return true;
}

bool AggressivePromotion::loop_may_write(const Loop *L, const Value *p) {
	MayWriteAnalyzer &MWA = getAnalysis<MayWriteAnalyzer>();
	assert(getAnalysisIfAvailable<AdvancedAlias>() == NULL);

	ConstFuncSet visited_funcs;
	// Loops over all BBs in this loop and its subloops. 
	for (Loop::block_iterator bi = L->block_begin(); bi != L->block_end(); ++bi) {
		BasicBlock *bb = *bi;
		for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
			if (MWA.may_write(ins, p, visited_funcs))
				return true;
		}
	}
	return false;
}
