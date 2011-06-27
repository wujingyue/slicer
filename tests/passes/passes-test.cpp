#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#include "pass-b.h"
namespace slicer {

	struct PassesTest: public ModulePass {

		static char ID;

		PassesTest(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void releaseMemory();
	};
}
using namespace slicer;

static RegisterPass<PassesTest> X(
		"passes-test",
		"",
		false,
		false);

char PassesTest::ID = 0;

bool PassesTest::runOnModule(Module &M) {
	PassB &PB = getAnalysis<PassB>();
	PB.runOnModule(M);
	return false;
}

void PassesTest::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<PassB>();
	ModulePass::getAnalysisUsage(AU);
}

void PassesTest::releaseMemory() {
	errs() << "PassesTest::releaseMemory\n";
}
