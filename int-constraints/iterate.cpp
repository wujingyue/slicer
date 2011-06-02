#include "bc2bdd/BddAliasAnalysis.h"
using namespace repair;

#include "llvm/Support/CommandLine.h"
using namespace llvm;

#include "capture.h"
#include "solve.h"
#include "adv-alias.h"
#include "iterate.h"

namespace {

	static RegisterPass<slicer::Iterate> X(
			"adv-alias",
			"Iterative alias analysis",
			false,
			false); // not an analysis
	static cl::opt<bool> RunTest(
			"test",
			cl::desc("Whether to run tests"),
			cl::init(false));
}

namespace slicer {

	bool Iterate::runOnModule(Module &M) {

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

		run_tests(M);

		return false;
	}

	void Iterate::test1(Module &M) {
		ObjectID &OI = getAnalysis<ObjectID>();
		SolveConstraints &SC = getAnalysis<SolveConstraints>();
		Value *v1 = OI.getValue(3138);
		Value *v2 = OI.getValue(3204);
		errs() << "may: " << SC.may_equal(v1, v2) << "\n";
	}

	void Iterate::test2(Module &M) {
		ObjectID &OI = getAnalysis<ObjectID>();
		SolveConstraints &SC = getAnalysis<SolveConstraints>();
		Value *off1 = OI.getValue(9175);
		Value *len1 = OI.getValue(9179);
		Value *off2 = OI.getValue(17126);
		// Value *len2 = OI.getValue(17130);
		Clause *c = new Clause(new BoolExpr(
					CmpInst::ICMP_SLE,
					new Expr(Instruction::Add, new Expr(off1), new Expr(len1)),
					new Expr(off2)));
		errs() << "must: " << SC.provable(vector<const Clause *>(1, c)) << "\n";
	}

	void Iterate::run_tests(Module &M) {
		// test1(M);
		test2(M);
	}

	void Iterate::getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
		AU.addRequired<ObjectID>();
		AU.addRequired<CaptureConstraints>();
		AU.addRequired<SolveConstraints>();
		AU.addRequired<AdvancedAlias>();
		ModulePass::getAnalysisUsage(AU);
	}

	char Iterate::ID = 0;
}
