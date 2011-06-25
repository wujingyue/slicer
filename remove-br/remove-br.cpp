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
#include "../int-constraints/iterate.h"
#include "../int-constraints/capture.h"
#include "../int-constraints/solve.h"
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
	bool changed = false;
	forallfunc(M, f) {
		BasicBlock *unreachable_bb = NULL;
		forall(Function, bb, *f) {
			CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
			if (CC.is_unreachable(bb)) {
				unreachable_bb = bb;
				break;
			}
		}
		forall(Function, bb, *f) {
			// TODO: We could do the same thing for SwitchInsts too. 
			if (BranchInst *bi = dyn_cast<BranchInst>(bb->getTerminator()))
				changed |= try_remove_branch(bi, unreachable_bb);
		}
		/*
		 * We added the unreachable BB after the loop, because it's a little
		 * dangerous to change the function while iterating through it.
		 *
		 * Also, if we reused an unreachable BB in the function rather than
		 * created a new one, we don't need to worry about parenting the BB. 
		 */
		if (unreachable_bb && unreachable_bb->getParent() == NULL)
			f->getBasicBlockList().push_back(unreachable_bb);
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
	bi->setSuccessor(i, unreachable_bb);
	return true;
}

bool RemoveBranch::try_remove_branch(
		BranchInst *bi, BasicBlock *&unreachable_bb) {

	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();

	if (bi->isUnconditional())
		return false;
	
	Value *cond = bi->getCondition();
	if (!CC.is_constant(cond))
		return false;
	
	const Use *use_cond = &bi->getOperandUse(0);
	// Remove the false branch if always true. 
	if (SC.provable(
				CmpInst::ICMP_EQ, use_cond, ConstantInt::getTrue(getGlobalContext())))
		return remove_branch(bi, 1, unreachable_bb);
	// Remove the true branch if always false. 
	if (SC.provable(
				CmpInst::ICMP_EQ, use_cond, ConstantInt::getFalse(getGlobalContext())))
		return remove_branch(bi, 0, unreachable_bb);
	// Do nothing if we cannot infer anything. 
	return false;
}
