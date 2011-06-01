#include "bc2bdd/BddAliasAnalysis.h"
using namespace repair;

#include "capture.h"
#include "solve.h"
#include "adv-alias.h"

namespace slicer {

	bool AdvancedAlias::runOnModule(Module &M) {
		return false;
	}

	void AdvancedAlias::print(raw_ostream &O, const Module *M) const {
		// Not sure what to do. 
	}

	void AdvancedAlias::getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
		AU.addRequired<BddAliasAnalysis>();
		AU.addRequired<CaptureConstraints>();
		AU.addRequired<SolveConstraints>();
		ModulePass::getAnalysisUsage(AU);
	}

	char AdvancedAlias::ID = 0;
}
