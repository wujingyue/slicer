#include "bc2bdd/BddAliasAnalysis.h"
using namespace repair;

#include "capture.h"
#include "solve.h"
#include "adv-alias.h"

namespace slicer {

	bool AdvancedAlias::runOnModule(Module &M) {
		CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
		SolveConstraints &SC = getAnalysis<SolveConstraints>();
		CC.replace_aa(&getAnalysis<AdvancedAlias>());
		unsigned n_constraints;
		do {
			errs() << "Iterating...\n";
			n_constraints = CC.get_num_constraints();
			CC.runOnModule(M);
			SC.runOnModule(M);
		} while (CC.get_num_constraints() == n_constraints);
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

	AliasAnalysis::AliasResult AdvancedAlias::alias(
			const Value *V1, unsigned V1Size,
			const Value *V2, unsigned V2Size) {
		BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
		if (BAA.alias(V1, V1Size, V2, V2Size) == NoAlias)
			return NoAlias;
		SolveConstraints &SC = getAnalysis<SolveConstraints>();
		if (SC.may_equal(V1, V2))
			return MayAlias;
		else
			return NoAlias;
	}

	char AdvancedAlias::ID = 0;
}
