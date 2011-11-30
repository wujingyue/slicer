/**
 * Author: Jingyue
 */

#include "llvm/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#include "common/typedefs.h"
using namespace rcs;

#include "slicer/adv-alias.h"
#include "slicer/test-utils.h"
#include "slicer/solve.h"
#include "int-test.h"
using namespace slicer;

void IntTest::test_blackscholes(Module &M) {
	AliasAnalysis &AA = getAnalysis<AdvancedAlias>();

	Value *prices = M.getNamedGlobal("prices");
	assert(prices);

	InstList loads;
	for (Value::use_iterator ui = prices->use_begin();
			ui != prices->use_end(); ++ui) {
		if (LoadInst *li = dyn_cast<LoadInst>(*ui)) {
			Function *f = li->getParent()->getParent();
			if (f->getName().startswith("_Z9bs_threadPv.SLICER"))
				loads.push_back(li);
		}
	}

	InstList geps;
	for (size_t i = 0; i < loads.size(); ++i) {
		Instruction *li = loads[i];
		for (Value::use_iterator ui = li->use_begin(); ui != li->use_end(); ++ui) {
			if (GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(*ui))
				geps.push_back(gep);
		}
	}

	dbgs() << "# of geps = " << geps.size() << "\n";
	for (size_t i = 0; i < geps.size(); ++i) {
		for (size_t j = i + 1; j < geps.size(); ++j) {
			errs() << "Gep " << i << " != Gep " << j << "? ...";
			AliasAnalysis::AliasResult res = AA.alias(geps[i], geps[j]);
			assert(res == AliasAnalysis::NoAlias);
			print_pass(errs());
		}
	}
}
