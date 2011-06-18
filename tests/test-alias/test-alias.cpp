/**
 * Author: Jingyue
 */

#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/DominatorInternals.h"
#include "common/include/util.h"
#include "idm/id.h"
using namespace llvm;

#include "bc2bdd/BddAliasAnalysis.h"
using namespace repair;

namespace slicer {

	struct TestAlias: public ModulePass {

		static char ID;

		TestAlias(): ModulePass(&ID) {}

		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
	};
}

namespace slicer {

	void TestAlias::getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
		AU.addRequired<ObjectID>();
		AU.addRequired<BddAliasAnalysis>();
		ModulePass::getAnalysisUsage(AU);
	}

	bool TestAlias::runOnModule(Module &M) {
		errs() << "===== test =====\n";
		ObjectID &OI = getAnalysis<ObjectID>();
		BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
		LoadInst *li = dyn_cast<LoadInst>(OI.getInstruction(2548));
		StoreInst *si = dyn_cast<StoreInst>(OI.getInstruction(1007));
		assert(li && si);
		errs() << *li << "\n" << *si << "\n";
		errs() << BAA.alias(
				li->getPointerOperand(), 0,
				si->getPointerOperand(), 0) << "\n";
		return false;
	}

	char TestAlias::ID = 0;
}

namespace {

	static RegisterPass<slicer::TestAlias> X(
			"test-alias",
			"Test aliasing",
			false,
			false);
}
