/**
 * Author: Jingyue
 */

#define DEBUG_TYPE "reducer"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/Debug.h"
#include "common/util.h"
#include "common/callgraph-fp.h"
#include "common/InitializePasses.h"
#include "bc2bdd/InitializePasses.h"
#include "slicer/InitializePasses.h"
using namespace llvm;

#include "slicer/aggressive-promotion.h"
#include "slicer/clone-info-manager.h"
#include "slicer/landmark-trace.h"
using namespace slicer;

#include "bc2bdd/BddAliasAnalysis.h"
using namespace bc2bdd;

INITIALIZE_PASS_BEGIN(AggressivePromotion, "aggressive-promotion",
		"A reducer running before the integer constraint solver", false, false)
INITIALIZE_PASS_DEPENDENCY(DominatorTree)
INITIALIZE_PASS_DEPENDENCY(LoopInfo)
INITIALIZE_PASS_DEPENDENCY(LandmarkTrace)
INITIALIZE_PASS_DEPENDENCY(CloneInfoManager)
INITIALIZE_PASS_DEPENDENCY(BddAliasAnalysis)
INITIALIZE_PASS_DEPENDENCY(CallGraphFP)
INITIALIZE_PASS_END(AggressivePromotion, "aggressive-promotion",
		"A reducer running before the integer constraint solver", false, false)

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
	AU.addRequired<BddAliasAnalysis>();
	AU.addRequired<CallGraphFP>();
}

AggressivePromotion::AggressivePromotion(): LoopPass(ID) {
	initializeAggressivePromotionPass(*PassRegistry::getPassRegistry());
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
		return !may_write(L, li->getPointerOperand());

	return isa<BinaryOperator>(ins) || isa<CastInst>(ins) ||
		isa<SelectInst>(ins) || isa<GetElementPtrInst>(ins) || isa<CmpInst>(ins) ||
		isa<InsertElementInst>(ins) || isa<ExtractElementInst>(ins) ||
		isa<ShuffleVectorInst>(ins);
}

bool AggressivePromotion::is_safe_to_execute_unconditionally(
		Loop *L, Instruction *ins) {
	if (ins->isSafeToSpeculativelyExecute())
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

bool AggressivePromotion::may_write(const Loop *L, const Value *p) {
	ConstFuncSet visited_funcs;
	// Loops over all BBs in this loop and its subloops. 
	for (Loop::block_iterator bi = L->block_begin(); bi != L->block_end(); ++bi) {
		BasicBlock *bb = *bi;
		for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
			if (may_write(ins, p, visited_funcs))
				return true;
		}
	}
	return false;
}

bool AggressivePromotion::may_write(
		const Instruction *i, const Value *q, ConstFuncSet &visited_funcs) {
	AliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
	CallGraphFP &CG = getAnalysis<CallGraphFP>();
	
	if (const StoreInst *si = dyn_cast<StoreInst>(i)) {
		if (BAA.alias(si->getPointerOperand(), 0, q, 0) != AliasAnalysis::NoAlias)
			return true;
	}
	
	// If <i> is a function call, go into the function. 
	if (is_call(i)) {
		FuncList callees = CG.get_called_functions(i);
		for (size_t j = 0; j < callees.size(); ++j) {
			if (may_write(callees[j], q, visited_funcs))
				return true;
		}
	}

	return false;
}

bool AggressivePromotion::may_write(
		const Function *f, const Value *q, ConstFuncSet &visited_funcs) {
	if (visited_funcs.count(f))
		return false;
	visited_funcs.insert(f);
	// FIXME: need function summary
	// For now, we assume external functions don't write to <q>. 
	if (f->isDeclaration())
		return false;
	forallconst(Function, bi, *f) {
		forallconst(BasicBlock, ii, *bi) {
			if (may_write(ii, q, visited_funcs))
				return true;
		}
	}
	return false;
}
