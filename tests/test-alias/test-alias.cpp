/**
 * Author: Jingyue
 */

#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/DominatorInternals.h"
#include "common/include/util.h"
#include "common/id-manager/IDAssigner.h"
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
		AU.addRequired<IDAssigner>();
		AU.addRequired<AliasAnalysis>();
		AU.addRequired<BddAliasAnalysis>();
		ModulePass::getAnalysisUsage(AU);
	}

	bool TestAlias::runOnModule(Module &M) {
		errs() << "===== test =====\n";
		IDAssigner &IDA = getAnalysis<IDAssigner>();
		BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
#if 0
		// test-malloc
		const Value *v1 = IDA.getValue(13);
		const Value *v2 = IDA.getValue(38);
#endif
#if 0
		// test-array-3
		const Value *v1 = IDA.getValue(12);
		const Value *v2 = IDA.getValue(19);
#endif
#if 1
		// test-array-2
		const Value *v1 = IDA.getValue(8);
		const Value *v2 = IDA.getValue(13);
#endif
		assert(v1 && v2);
		errs() << *v1 << "\n" << *v2 << "\n";
		errs() << BAA.alias(v1, 0, v2, 0) << "\n";
		return false;
	}

	char TestAlias::ID = 0;
}

namespace {

	static RegisterPass<slicer::TestAlias> X("test-alias", "Test aliasing");
}
