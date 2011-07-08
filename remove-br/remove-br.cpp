/**
 * Author: Jingyue
 *
 * If a conditional branch is guaranteed to be true/false, we redirect its
 * false/true branch to an unreachable BB. This simplification may make
 * some BBs unreachable, and thus eliminate them later. 
 */

#define DEBUG_TYPE "remove-br"

#include "llvm/LLVMContext.h"
#include "llvm/ADT/Statistic.h"
using namespace llvm;

#include "remove-br.h"
#include "../int/iterate.h"
#include "../int/capture.h"
#include "../int/solve.h"
using namespace slicer;

static RegisterPass<RemoveBranch> X(
		"remove-br",
		"Remove unreachable branches according to int-constraints",
		false,
		false);

STATISTIC(BranchesRemoved, "Number of branches removed");

char RemoveBranch::ID = 0;

void RemoveBranch::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.addRequired<Iterate>();
	AU.addRequired<CaptureConstraints>();
	AU.addRequired<SolveConstraints>();
	ModulePass::getAnalysisUsage(AU);
}

bool RemoveBranch::runOnModule(Module &M) {
	
	// TODO: We could do the same thing for SwitchInsts too. 
	/*
	 * <should_remove_branch> queries the solver. Therefore, we shouldn't
	 * modify the module while calling this function. 
	 */
	vector<pair<BranchInst *, unsigned> > to_remove;
	forallbb(M, bb) {
		if (BranchInst *bi = dyn_cast<BranchInst>(bb->getTerminator()))
			prepare_remove_branch(bi, to_remove);
	}

	/* Remove those unreachable branches. */
	/* <unreachable_bbs> is used as a cache. */
	DenseMap<Function *, BasicBlock *> unreachable_bbs;
	bool changed = false;
	for (size_t i = 0; i < to_remove.size(); ++i) {

		BranchInst *bi = to_remove[i].first;
		unsigned which = to_remove[i].second;
		
		Function *f = bi->getParent()->getParent();
		// Note that we are using (BasicBlock *&).
		BasicBlock *&unreachable_bb = unreachable_bbs[f];
		// If not in the cache, we try to find an existing unreachable BB.
		if (!unreachable_bb) {
			forall(Function, bb, *f) {
				CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
				if (CC.is_unreachable(bb)) {
					unreachable_bb = bb;
					break;
				}
			}
		}
		changed |= remove_branch(bi, which, unreachable_bb);
	}

	return changed;
}

bool RemoveBranch::remove_branch(
		TerminatorInst *bi, unsigned i, BasicBlock *&unreachable_bb) {

	assert(i < bi->getNumSuccessors());
	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
	/*
	 * There may be multiple unreachable BBs in a function. Therefore, 
	 * we should call function is_unreachable instead of simply comparing
	 * the sucessor with <unreachable_bb>.
	 */
	if (CC.is_unreachable(bi->getSuccessor(i)))
		return false;

	errs() << "=== remove_branch ===\n";
	++BranchesRemoved;
	// Create the unreachable BB if necessary. 
	if (!unreachable_bb) {
		// We will insert <unreachable_bb> into the function later. 
		unreachable_bb = BasicBlock::Create(getGlobalContext(), "unreachable");
		// Insert an llvm.trap in the unreachable BB. 
		Function *trap = Intrinsic::getDeclaration(
				bi->getParent()->getParent()->getParent(), Intrinsic::trap);
		CallInst::Create(trap, "", unreachable_bb);
		new UnreachableInst(getGlobalContext(), unreachable_bb);
	}
	
	// If a PHINode in the old successor has an incoming value from the
	// current BB, remove that incoming value. 
	BasicBlock *old_succ = bi->getSuccessor(i);
	for (BasicBlock::iterator ii = old_succ->begin();
			old_succ->getFirstNonPHI() != ii; ++ii) {
		PHINode *phi = dyn_cast<PHINode>(ii);
		assert(phi && "All instructions before getFirstNonPHI should be PHINodes.");
		int idx = phi->getBasicBlockIndex(bi->getParent());
		if (idx >= 0)
			phi->removeIncomingValue(idx);
	}
	// Make the unreachable BB the new successor. 
	// An unreachable BB doesn't have any PHINodes. 
	bi->setSuccessor(i, unreachable_bb);
	return true;
}

void RemoveBranch::prepare_remove_branch(
		BranchInst *bi, vector<pair<BranchInst *, unsigned> > &to_remove) {

	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();

	if (bi->isUnconditional())
		return;
	
	Value *cond = bi->getCondition();
	if (!CC.is_constant(cond))
		return;
	
	const Use *use_cond = &bi->getOperandUse(0);
	// Remove the false branch if always true. 
	if (SC.provable(
				CmpInst::ICMP_EQ, use_cond, ConstantInt::getTrue(getGlobalContext())))
		to_remove.push_back(make_pair(bi, 1));
	// Remove the true branch if always false. 
	if (SC.provable(
				CmpInst::ICMP_EQ, use_cond, ConstantInt::getFalse(getGlobalContext())))
		to_remove.push_back(make_pair(bi, 0));
}
