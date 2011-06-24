/**
 * Author: Jingyue
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
			// TODO: We could do the same thing for SwitchInsts too. 
			if (BranchInst *bi = dyn_cast<BranchInst>(bb->getTerminator()))
				changed |= try_remove_branch(bi, unreachable_bb);
		}
		// We added the unreachable BB after the loop, because it's a little
		// dangerous to change the function while iterating through it. 
		if (unreachable_bb)
			f->getBasicBlockList().push_back(unreachable_bb);
	}
	return changed;
}

void RemoveBranch::remove_branch(
		TerminatorInst *bi, unsigned i, BasicBlock *&unreachable_bb) {
	assert(i < bi->getNumSuccessors());
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
	
	const IntegerType *int_type = IntegerType::get(getGlobalContext(), 32);
	const Use *use_cond = &bi->getOperandUse(0);
	if (SC.must_equal(use_cond, ConstantInt::get(int_type, 1))) {
		// Remove the false branch. 
		remove_branch(bi, 1, unreachable_bb);
		return true;
	}
	if (SC.must_equal(use_cond, ConstantInt::get(int_type, 0))) {
		// Remove the true branch.
		remove_branch(bi, 0, unreachable_bb);
		return true;
	}

	// Unknown. 
	return false;
}
