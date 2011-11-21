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
	LoopInfo &LI = getAnalysis<LoopInfo>();

	BasicBlock *preheader = L->getLoopPreheader();
	if (!preheader)
		return false;
	
	InstList to_promote;
	for (Loop::block_iterator bi = L->block_begin(); bi != L->block_end(); ++bi) {
		BasicBlock *bb = *bi;
		// Ignore blocks in subloops.
		if (LI.getLoopFor(bb) == L) {
			for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
				if (LoadInst *li = dyn_cast<LoadInst>(ins)) {
					// Try hoisting this load instruction. 
					if (should_promote(li, L))
						to_promote.push_back(li);
				}
			}
		}
	}

	for (size_t i = 0; i < to_promote.size(); ++i) {
		// Promote to_promote[i] to <preheader>. 
		// Copied from LICM.cpp, around Line 607. 
		Instruction *ins = to_promote[i];
		DEBUG(dbgs() << "=== Promoting " << *ins << " ===\n";);
		ins->removeFromParent();
		preheader->getInstList().insert(preheader->getTerminator(), ins);
	}

	return to_promote.size() > 0;
}

bool AggressivePromotion::should_promote(LoadInst *li, Loop *L) {

	Value *p = li->getPointerOperand();
	// TODO: For performance sake, we work on global variables only. 
	if (!isa<GlobalVariable>(p))
		return false;
	
	// Check whether the loop itself will write to <p>. 
	if (may_write(L, p))
		return false;
	
	// TODO: Check concurrent regions. 
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

bool AggressivePromotion::path_may_write(
		const Instruction *i1, const Instruction *i2, const Value *p) {
	// TODO: To implement
	return false;
}
