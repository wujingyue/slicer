#include "common/include/util.h"
using namespace llvm;

#include "capture.h"
#include "solve.h"

namespace {

	static RegisterPass<slicer::SolveConstraints> X(
			"solve-constraints",
			"Solve captured constraints using STP",
			false,
			true); // is analysis
}

namespace slicer {

	bool SolveConstraints::runOnModule(Module &M) {
		// Do nothing. 
		return false;
	}

	void SolveConstraints::print(raw_ostream &O, const Module *M) const {
		// Don't know what to do. 
	}

	void SolveConstraints::getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
		// Will be used somewhere not in runOnModule. 
		AU.addRequiredTransitive<CaptureConstraints>();
		ModulePass::getAnalysisUsage(AU);
	}

	bool SolveConstraints::satisfiable(const vector<Clause *> &more_clauses) {
		assert_not_implemented();
	}

	char SolveConstraints::ID = 0;
}
