/**
 * Author: Jingyue
 */

#define DEBUG_TYPE "reducer"

#include <sstream>
using namespace std;

#include "llvm/LLVMContext.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/Debug.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Target/TargetData.h"
#include "common/util.h"
using namespace llvm;

#include "slicer/iterate.h"
#include "slicer/capture.h"
#include "slicer/solve.h"
#include "slicer/max-slicing.h"
#include "slicer/constantizer.h"
using namespace slicer;

static RegisterPass<Constantizer> X("constantize",
		"Replace variables with constants whenever possible and "
		"remove unreachable branches according to int-constraints");

STATISTIC(VariablesConstantized, "Number of variables constantized");

char Constantizer::ID = 0;

void Constantizer::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.addRequired<TargetData>();
	AU.addRequired<Iterate>();
	AU.addRequired<CaptureConstraints>();
	AU.addRequired<SolveConstraints>();
	ModulePass::getAnalysisUsage(AU);
}

bool Constantizer::constantize(Module &M) {
	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
	SolveConstraints &SC = getAnalysis<SolveConstraints>();

	vector<pair<Value *, ConstantInt *> > to_replace;
	// TODO: We consider only constants for now. 
	const ValueSet &constants = CC.get_fixed_integers();
	for (ValueSet::const_iterator it = constants.begin();
			it != constants.end(); ++it) {
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
		Value *v = to_replace[i].first;
		ConstantInt *c = to_replace[i].second;
		
		vector<Use *> local;
		// Don't replace uses while iterating. 
		// Put them to a local list first. 
		bool already_asserted = false;
		for (Value::use_iterator ui = v->use_begin(); ui != v->use_end(); ++ui) {
			if (Instruction *ins = dyn_cast<Instruction>(*ui)) {
				if (CaptureConstraints::is_slicer_assert_eq(ins, NULL, NULL)) {
					already_asserted = true;
					continue;
				}
			}
			local.push_back(&ui.getUse());
		}

		DEBUG(dbgs() << "=== replacing with a constant ===\n";);
		DEBUG(dbgs() << "Constant = " << *c << "\n";);
		if (Instruction *ins = dyn_cast<Instruction>(v)) {
			DEBUG(dbgs() << ins->getParent()->getParent()->getName() << ":" <<
					*ins << "\n";);
		} else if (Argument *arg = dyn_cast<Argument>(v)) {
			DEBUG(dbgs() << arg->getParent()->getName() << ":" << *arg << "\n";);
		} else {
			DEBUG(dbgs() << *v << "\n";);
		}
	
		// Give up if <v> is a pointer. 
		if (!already_asserted && local.size() > 0) {
			// Find the position to add slicer_assert_eq
			Instruction *pos = NULL;
			if (Instruction *ins = dyn_cast<Instruction>(v)) {
				BasicBlock::iterator next = ins; ++next;
				for (; next != ins->getParent()->end() && isa<PHINode>(next); ++next);
				// Stops if <next> reaches the end or it's not a PHINode. 
				if (next == ins->getParent()->end()) {
					assert(isa<InvokeInst>(ins));
					next = cast<InvokeInst>(ins)->getNormalDest()->getFirstNonPHI();
				}
				pos = next;
			} else if (Argument *arg = dyn_cast<Argument>(v)) {
				pos = arg->getParent()->begin()->getFirstNonPHI();
			}

			if (pos) {
				// Add slicer_assert_eq
				Constant *the_slicer_assert = get_slicer_assert(M, v->getType());
				vector<Value *> actual_args;
				if (isa<IntegerType>(v->getType())) {
					actual_args.push_back(v);
					// v and c may not be of the same type, because they are retrieved
					// from the union find set.
					actual_args.push_back(ConstantInt::getSigned(v->getType(),
								(int)c->getSExtValue()));
				} else {
					actual_args.push_back(v);
					assert(isa<PointerType>(v->getType()));
					const PointerType *ptr_type = cast<PointerType>(v->getType());
					Value *ptr_c = new IntToPtrInst(c, ptr_type, "", pos);
					actual_args.push_back(ptr_c);
				}

				assert(the_slicer_assert);
				CallInst::Create(the_slicer_assert,
						actual_args.begin(), actual_args.end(), "", pos);
			}
		}

		DEBUG(dbgs() << "Uses:\n";);
		// FIXME: Integer types in the solver may not be consistent with there
		// real types. Therefore, we create new ConstantInt's with respect to
		// the correct integer types. 
		bool locally_changed = false;
		for (size_t j = 0; j < local.size(); ++j) {
			const Type *type = local[j]->get()->getType();
			if (const IntegerType *int_type = dyn_cast<IntegerType>(type)) {
#if 0
				/*
				 * FIXME: This is a quick hack to prevent the constantizer from
				 * replacing branch conditions so as to keep BranchInsts. 
				 * A better way should be annotating constants. 
				 */
				if (int_type->getBitWidth() == 1)
					continue;
#endif
				// Signed values. 
				int64_t svalue = c->getSExtValue();
				DEBUG(dbgs() << *local[j]->getUser() << "\n";);
				local[j]->set(ConstantInt::getSigned(int_type, svalue));
				locally_changed = true;
				DEBUG(dbgs() << "afterwards:" << *local[j]->getUser() << "\n";);
			} else if (const PointerType *ptr_type = dyn_cast<PointerType>(type)) {
				if (c->isZero()) {
					DEBUG(dbgs() << *local[j]->getUser() << "\n";);
					local[j]->set(ConstantPointerNull::get(ptr_type));
					locally_changed = true;
					DEBUG(dbgs() << "afterwards:" << *local[j]->getUser() << "\n";);
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

void Constantizer::setup(Module &M) {
	// Do nothing for now. 
}

Function *Constantizer::get_slicer_assert(Module &M, const Type *type) {
	if (slicer_assert_eq.count(type))
		return slicer_assert_eq.lookup(type);

	const Type *void_ty = Type::getVoidTy(M.getContext());
	vector<const Type *> arg_types(2, type);
	FunctionType *func_type = FunctionType::get(void_ty, arg_types, false);
	Function *f = Function::Create(func_type,
			GlobalVariable::ExternalLinkage, "slicer_assert_eq", &M);
	slicer_assert_eq[type] = f;
	return f;
}

bool Constantizer::runOnModule(Module &M) {
	SolveConstraints &SC = getAnalysis<SolveConstraints>();

	setup(M);
	/*
	 * NOTE: Constantize the module before removing branches. 
	 * The former does not change the CFG. 
	 */
	bool changed = false;
	
	TimerGroup tg("Constantizer");
	Timer tmr_identify("Identify", tg);
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
