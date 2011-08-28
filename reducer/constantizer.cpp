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

#include "constantizer.h"
#include "int/iterate.h"
#include "int/capture.h"
#include "int/solve.h"
#include "max-slicing/max-slicing.h"
using namespace slicer;

static RegisterPass<Constantizer> X(
		"post-reduce",
		"Replace variables with constants whenever possible and "
		"remove unreachable branches according to int-constraints");

STATISTIC(VariablesConstantized, "Number of variables constantized");

char Constantizer::ID = 0;

void Constantizer::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.addRequired<Iterate>();
	AU.addRequired<CaptureConstraints>();
	AU.addRequired<SolveConstraints>();
	ModulePass::getAnalysisUsage(AU);
}

bool Constantizer::constantize(Module &M) {
	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
	SolveConstraints &SC = getAnalysis<SolveConstraints>();

	vector<pair<const Value *, ConstantInt *> > to_replace;
	// TODO: consider only constants. 
	const ConstValueSet &constants = CC.get_fixed_integers();
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
				local[j]->set(ConstantInt::getSigned(int_type, svalue));
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

bool Constantizer::runOnModule(Module &M) {
	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	/*
	 * NOTE: Constantize the module before removing branches. 
	 * The former does not change the CFG. 
	 */
	bool changed = false;
	
	TimerGroup tg("Constantize");
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

	return changed;
}
