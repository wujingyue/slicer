/**
 * Author: Jingyue
 */

#define DEBUG_TYPE "reducer"

#include "llvm/LLVMContext.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/Debug.h"
#include "llvm/ADT/Statistic.h"
#include "common/include/util.h"
using namespace llvm;

#include "post-reducer.h"
#include "int/iterate.h"
#include "int/capture.h"
#include "int/solve.h"
#include "max-slicing/max-slicing.h"
using namespace slicer;

static RegisterPass<PostReducer> X(
		"post-reduce",
		"Replace variables with constants whenever possible and "
		"remove unreachable branches according to int-constraints");

STATISTIC(BranchesRemoved, "Number of branches removed");
STATISTIC(VariablesConstantized, "Number of variables constantized");

char PostReducer::ID = 0;

void PostReducer::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.addRequired<Iterate>();
	AU.addRequired<CaptureConstraints>();
	AU.addRequired<SolveConstraints>();
	ModulePass::getAnalysisUsage(AU);
}

bool PostReducer::constantize(Module &M) {

	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
	SolveConstraints &SC = getAnalysis<SolveConstraints>();

	vector<pair<const Value *, ConstantInt *> > to_replace;
	// TODO: consider only constants. 
	const ConstValueSet &constants = CC.get_integers();
	forallconst(ConstValueSet, it, constants) {
		// Skip if already a constant. 
		if (isa<Constant>(*it))
			continue;
		assert(isa<Instruction>(*it) || isa<Argument>(*it));
		// <v> may not be a ConstantInt, because the solver treats pointers as
		// integers, and may put them in one equivalent class. 
#if 0
		if (!isa<IntegerType>((*it)->getType()))
			continue;
#endif
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

		DEBUG(dbgs() << "=== replacing with a constant ===\n";);
		DEBUG(dbgs() << "Constant = " << *to_replace[i].second << "\n";);
		if (const Instruction *ins = dyn_cast<Instruction>(v)) {
			DEBUG(dbgs() << ins->getParent()->getParent()->getName() << ":" <<
					*ins << "\n";);
		} else if (const Argument *arg = dyn_cast<Argument>(v)) {
			DEBUG(dbgs() << arg->getParent()->getName() << ":" << *arg << "\n";);
		} else {
			DEBUG(dbgs() << *v << "\n";);
		}
	
		DEBUG(dbgs() << "Uses:\n";);
		// FIXME: Integer types in the solver may not be consistent with there
		// real types. Therefore, we create new ConstantInt's with respect to
		// the correct integer types. 
		bool locally_changed = false;
		for (size_t j = 0; j < local.size(); ++j) {
			const Type *type = local[j]->get()->getType();
			if (const IntegerType *int_type = dyn_cast<IntegerType>(type)) {
				/*
				 * FIXME: This is a quick hack to prevent the constantizer from
				 * replacing branch conditions so as to keep BranchInsts. 
				 * A better way should be annotating constants. 
				 */
				if (int_type->getBitWidth() == 1)
					continue;
				// Signed values. 
				int64_t svalue = to_replace[i].second->getSExtValue();
				DEBUG(dbgs() << *local[j]->getUser() << "\n";);
				local[j]->set(ConstantInt::get(int_type, svalue, true));
				locally_changed = true;
			} else if (const PointerType *ptr_type = dyn_cast<PointerType>(type)) {
				if (to_replace[i].second->isZero()) {
					DEBUG(dbgs() << *local[j]->getUser() << "\n";);
					local[j]->set(ConstantPointerNull::get(ptr_type));
					locally_changed = true;
				}
			} else {
				assert(false && "This value is neither an integer or a pointer");
			}
		}
		
		if (locally_changed) {
			++VariablesConstantized;
		}
		changed |= locally_changed;
	}

	return changed;
}

bool PostReducer::runOnModule(Module &M) {

	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	/*
	 * NOTE: Constantize the module before removing branches. 
	 * The former does not change the CFG. 
	 */
	bool changed = false;
	
	TimerGroup tg("Post-reducer");
	Timer tmr_identify("Identify", tg);
	Timer tmr_remove_br("Remove branches", tg);
	Timer tmr_constantize("Constantize", tg);

	// Let SolveConstraints identify all constants. 
	tmr_identify.startTimer();
	dbgs() << "=== Start identifying fixed values... ===\n";
	SC.identify_fixed_values();
	dbgs() << "=== Finished ===\n";
	tmr_identify.stopTimer();

	// Replace variables with ConstantInts whenever possible.
	tmr_constantize.startTimer();
	changed |= constantize(M);
	tmr_constantize.stopTimer();
	
	// Remove unreachable branches. 
	tmr_remove_br.startTimer();
	changed |= remove_branches(M);
	tmr_remove_br.stopTimer();

	return changed;
}

bool PostReducer::remove_branches(Module &M) {

	dbgs() << "Try removing branches... ";
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
	dbgs() << "Done\n";

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
				if (MaxSlicing::is_unreachable(bb)) {
					unreachable_bb = bb;
					break;
				}
			}
		}
		changed |= remove_branch(bi, which, unreachable_bb);
	}

	return changed;
}

bool PostReducer::remove_branch(
		TerminatorInst *ti, unsigned i, BasicBlock *&unreachable_bb) {

	assert(i < ti->getNumSuccessors());
	// Already unreachable. 
	if (MaxSlicing::is_unreachable(ti->getSuccessor(i)))
		return false;

	DEBUG(dbgs() << "=== remove_branch ===\n";);
	DEBUG(dbgs() << "Branch " << i << " of" << *ti << "\n";);
	++BranchesRemoved;
	// Create the unreachable BB if necessary. 
	if (!unreachable_bb) {
		unreachable_bb = MaxSlicing::create_unreachable(
				ti->getParent()->getParent());
	}
	
	// If a PHINode in the old successor has an incoming value from the
	// current BB, remove that incoming value. 
	BasicBlock *old_succ = ti->getSuccessor(i);
	for (BasicBlock::iterator ii = old_succ->begin();
			old_succ->getFirstNonPHI() != ii; ++ii) {
		PHINode *phi = dyn_cast<PHINode>(ii);
		assert(phi && "All instructions before getFirstNonPHI should be PHINodes.");
		int idx = phi->getBasicBlockIndex(ti->getParent());
		if (idx >= 0)
			phi->removeIncomingValue(idx);
	}
	// Make the unreachable BB the new successor. 
	// An unreachable BB doesn't have any PHINodes. 
	ti->setSuccessor(i, unreachable_bb);
	return true;
}

void PostReducer::prepare_remove_branch(
		BranchInst *bi, vector<pair<BranchInst *, unsigned> > &to_remove) {

	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();

	if (bi->isUnconditional())
		return;
	
	Value *cond = bi->getCondition();
	if (!CC.is_integer(cond))
		return;

	// If one of the branches is already removed, don't waste time do
	// it again. 
	if (MaxSlicing::is_unreachable(bi->getSuccessor(0)) ||
			MaxSlicing::is_unreachable(bi->getSuccessor(1)))
		return;
	
	// NOTE: Originally we query SolveConstraints with the condition. This
	// gives us extra constraints on the branches along the way. By using
	// <get_fixed_value>, we ignore these branch constraints. Is it good? 
	ConstantInt *ci = SC.get_fixed_value(cond);
	if (ci) {
		// Remove the true branch (branch 0) if always false. 
		// Remove the true branch (branch 1) if always true. 
		if (ci->isZero())
			to_remove.push_back(make_pair(bi, 0));
		else if (ci->isOne())
			to_remove.push_back(make_pair(bi, 1));
		else
			assert_unreachable();
	}
}