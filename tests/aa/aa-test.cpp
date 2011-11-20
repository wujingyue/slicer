/**
 * Author: Jingyue
 */

#include "common/IDAssigner.h"
using namespace llvm;

#include "bc2bdd/BddAliasAnalysis.h"
using namespace repair;

namespace slicer {
	struct AATest: public ModulePass {
		static char ID;

		AATest(): ModulePass(ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const {
			AU.setPreservesAll();
			AU.addRequired<IDAssigner>();
			AU.addRequired<BddAliasAnalysis>();
		}
	};
}
using namespace slicer;

static RegisterPass<AATest> X("aa-test",
		"Test alias analysis", false, true);

char AATest::ID = 0;

bool AATest::runOnModule(Module &M) {
	IDAssigner &IDA = getAnalysis<IDAssigner>();
	AliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();

	const Value *v1 = IDA.getValue(80);
	const Value *v2 = IDA.getValue(156);

	errs() << *v1 << "\n" << *v2 << "\n";
	errs() << BAA.alias(v1, v2) << "\n";
	return false;
}
