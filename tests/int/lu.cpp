#include "llvm/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Analysis/LoopInfo.h"
#include "common/util.h"
#include "common/exec-once.h"
using namespace llvm;

#include "slicer/adv-alias.h"
#include "slicer/test-utils.h"
#include "slicer/solve.h"
#include "int-test.h"
using namespace slicer;

void IntTest::test_lu_cont(const Module &M) {
	TestBanner X("LU");

	AdvancedAlias &AA = getAnalysis<AdvancedAlias>();
	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	ExecOnce &EO = getAnalysis<ExecOnce>();

	DenseMap<const Function *, const StoreInst *> accesses;
	for (Module::const_iterator f = M.begin(); f != M.end(); ++f) {
		if (EO.not_executed(f))
			continue;
		if (!starts_with(f->getName(), "SlaveStart.SLICER"))
			continue;

		bool found = false;
		LoopInfo &LI = getAnalysis<LoopInfo>(
				*const_cast<Function *>((const Function *)f));
		for (Function::const_iterator bb = f->begin(); bb != f->end(); ++bb) {
			if (LI.getLoopDepth(bb) == 0)
				continue;
			for (BasicBlock::const_iterator ins = bb->begin();
					ins != bb->end(); ++ins) {
				if (const StoreInst *si = dyn_cast<StoreInst>(ins)) {
					if (si->getOperand(0)->getType()->isDoubleTy()) {
						accesses[f] = si;
						found = true;
						break;
					}
				}
			}
			if (found)
				break;
		}
		assert(found);
	}

	for (DenseMap<const Function *, const StoreInst *>::iterator
			i = accesses.begin(); i != accesses.end(); ++i) {
		DenseMap<const Function *, const StoreInst *>::iterator j = i;
		for (++j; j != accesses.end(); ++j) {
			errs() << i->first->getName() << " and " << j->first->getName()
				<< " access disjoint regions?... ";
			SC.set_print_counterexample(true);
			assert(AA.alias(i->second->getPointerOperand(), 0,
					j->second->getPointerOperand(), 0) == AliasAnalysis::NoAlias);
			SC.set_print_counterexample(false);
			print_pass(errs());
		}
	}
}
