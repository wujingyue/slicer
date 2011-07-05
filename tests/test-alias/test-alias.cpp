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
		AU.addRequired<AliasAnalysis>();
		AU.addRequired<BddAliasAnalysis>();
		ModulePass::getAnalysisUsage(AU);
	}

	bool TestAlias::runOnModule(Module &M) {
		errs() << "===== test =====\n";
		ObjectID &OI = getAnalysis<ObjectID>();
		BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
		// AliasAnalysis &BAA = getAnalysis<AliasAnalysis>();
		const Value *v1 = OI.getValue(2670);
		const Value *v2 = OI.getValue(3296);
		assert(v1 && v2);
		errs() << *v1 << "\n" << *v2 << "\n";
		errs() << BAA.alias(v1, 0, v2, 0) << "\n";
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
