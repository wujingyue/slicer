#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#include "pass-a.h"
#include "pass-b.h"
using namespace slicer;

static RegisterPass<PassB> X(
		"pass-b",
		"",
		false,
		true);

char PassB::ID = 0;

PassB::PassB(): ModulePass(&ID) {
	errs() << "PassB::PassB\n";
}

bool PassB::runOnModule(Module &M) {
	PassA &PA = getAnalysis<PassA>();
	errs() << "PA.getSize() = " << PA.getSize() << "\n";
	return false;
}

void PassB::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<PassA>();
	ModulePass::getAnalysisUsage(AU);
}

void PassB::releaseMemory() {
	errs() << "PassB::releaseMemory\n";
}
