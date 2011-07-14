/**
 * Author: Jingyue
 *
 * If a conditional branch is guaranteed to be true/false, we redirect its
 * false/true branch to an unreachable BB. This simplification may make
 * some BBs unreachable, and thus eliminate them later. 
 */

#define DEBUG_TYPE "reducer"

#include "llvm/LLVMContext.h"
#include "llvm/Support/Timer.h"
#include "llvm/ADT/Statistic.h"
using namespace llvm;

#include "reducer.h"
#include "../int/iterate.h"
#include "../int/capture.h"
#include "../int/solve.h"
using namespace slicer;

static RegisterPass<Reducer> X(
		"reduce",
		"Replace variables with constants whenever possible and "
		"remove unreachable branches according to int-constraints");

STATISTIC(BranchesRemoved, "Number of branches removed");
STATISTIC(VariablesConstantized, "Number of variables constantized");

char Reducer::ID = 0;

void Reducer::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.addRequired<Iterate>();
	AU.addRequired<CaptureConstraints>();
	AU.addRequired<SolveConstraints>();
	ModulePass::getAnalysisUsage(AU);
}

bool Reducer::constantize(Module &M) {

	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
	SolveConstraints &SC = getAnalysis<SolveConstraints>();

	vector<pair<const Value *, ConstantInt *> > to_replace;
	const ConstValueSet &constants = CC.get_constants();
	forallconst(ConstValueSet, it, constants) {
		// Skip if already a constant. 
		if (isa<Constant>(*it))
			continue;
		assert(isa<Instruction>(*it) || isa<Argument>(*it));
		if (ConstantInt *ci = SC.get_fixed_value(*it))
			to_replace.push_back(make_pair(*it, ci));
	}

	bool changed = false;
	for (size_t i = 0; i < to_replace.size(); ++i) {
		const Value *v = to_replace[i].first;
		vector<Use *> local;
		// Don't replace uses while iterating. 
		// Put them to a local list first. 
		for (Value::use_const_iterator ui = v->use_begin();
				ui != v->use_end(); ++ui)
			local.push_back(&ui.getUse());
		if (local.size() > 0) {
			++VariablesConstantized;
			errs() << "=== replacing with a constant ===\n";
		}
		// FIXME: Integer types in the solver may not be consistent with there
		// real types. Therefore, we create new ConstantInt's with respect to
		// the correct integer types. 
		for (size_t j = 0; j < local.size(); ++j) {
			const IntegerType *int_type =
				dyn_cast<IntegerType>(local[j]->get()->getType());
#if 0
			// FIXME: This is a quick hack to prevent the constantizer from
			// replacing branch conditions so as to keep BranchInsts. 
			// A better way should be annotating constants. 
			if (int_type->getBitWidth() == 1)
				continue;
#endif
			// Signed values. 
			int64_t svalue = to_replace[i].second->getSExtValue();
			local[j]->set(ConstantInt::get(int_type, svalue, true));
			changed = true;
		}
	}

	return changed;
}

bool Reducer::runOnModule(Module &M) {

	/*
	 * NOTE: Constantize the module before removing branches. 
	 * The former does not change the CFG. 
	 */
	bool changed = false;
	
	TimerGroup tg("Reducer");
	Timer tmr_remove_br("Remove branches", tg);
	Timer tmr_constantize("Constantize", tg);

	// Replace variables with ConstantInts whenever possible.
	tmr_remove_br.startTimer();
	changed |= constantize(M);
	tmr_remove_br.stopTimer();
	
	// Remove unreachable branches. 
	tmr_constantize.startTimer();
	changed |= remove_branches(M);
	tmr_constantize.stopTimer();

	return changed;
}

bool Reducer::remove_branches(Module &M) {

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

bool Reducer::remove_branch(
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

void Reducer::prepare_remove_branch(
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
				CmpInst::ICMP_EQ, use_cond, ConstantInt::getTrue(bi->getContext())))
		to_remove.push_back(make_pair(bi, 1));
	// Remove the true branch if always false. 
	if (SC.provable(
				CmpInst::ICMP_EQ, use_cond, ConstantInt::getFalse(bi->getContext())))
		to_remove.push_back(make_pair(bi, 0));
}
