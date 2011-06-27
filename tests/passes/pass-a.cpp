#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#include "pass-a.h"
using namespace slicer;

static RegisterPass<PassA> X(
		"pass-a",
		"",
		false,
		true);

char PassA::ID = 0;

PassA::PassA(): ModulePass(&ID), size(0) {
	errs() << "PassA::PassA\n";
}

bool PassA::runOnModule(Module &M) {
	size = 1;
	return false;
}

void PassA::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	ModulePass::getAnalysisUsage(AU);
}

void PassA::releaseMemory() {
	errs() << "PassA::releaseMemory\n";
	size = 0;
}
