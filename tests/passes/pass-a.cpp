#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#include "pass-a.h"
using namespace slicer;

static RegisterPass<PassA> X(
		"pass-a",
		"Pass A",
		false,
		false);

char PassA::ID = 0;

PassA::PassA(): ModulePass(&ID), size(0) {
	errs() << "PassA::PassA\n";
}

bool PassA::runOnModule(Module &M) {
	errs() << "PassA::runOnModule\n";
	++size;
	errs() << "size = " << size << "\n";
	if (size > 4)
		return false;
	else
		return true;
}

void PassA::getAnalysisUsage(AnalysisUsage &AU) const {
	// AU.setPreservesAll();
	ModulePass::getAnalysisUsage(AU);
}

void PassA::releaseMemory() {
	errs() << "PassA::releaseMemory\n";
	size = 0;
}
