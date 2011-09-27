#include "slicer/query-translator.h"
using namespace slicer;

static RegisterPass<QueryTranslator> X("translate-queries",
		"Translate queries on the original program to "
		"those on the sliced program",
		false, true);

char QueryTranslator::ID = 0;

bool QueryTranslator::runOnModule(Module &M) {
	return false;
}

void QueryTranslator::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	ModulePass::getAnalysisUsage(AU);
}
