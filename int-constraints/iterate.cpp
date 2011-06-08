#include "bc2bdd/BddAliasAnalysis.h"
using namespace repair;

#include "llvm/Support/CommandLine.h"
#include "llvm/LLVMContext.h"
using namespace llvm;

#include "capture.h"
#include "solve.h"
#include "adv-alias.h"
#include "iterate.h"

namespace {

	static RegisterPass<slicer::Iterate> X(
			"iterate",
			"A iterator to provide more accurate analyses",
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
		AdvancedAlias &AAA = getAnalysis<AdvancedAlias>();
		CC.replace_aa(&AAA);
		unsigned n_constraints;
		do {
			n_constraints = CC.get_num_constraints();
			errs() << "Iterating... # of constraints = " << n_constraints << "\n";
			errs() << "AAA cache size = " << AAA.get_cache_size() << "\n";
			AAA.runOnModule(M); // Essentially clear the cache. 
			CC.runOnModule(M);
			SC.runOnModule(M);
			assert(CC.get_num_constraints() <= n_constraints &&
					"# of constraints should be reduced after an iteration");
		} while (CC.get_num_constraints() != n_constraints);

		if (RunTest)
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

	void Iterate::test3(Module &M) {
		ObjectID &OI = getAnalysis<ObjectID>();
		SolveConstraints &SC = getAnalysis<SolveConstraints>();
		Value *v1 = OI.getValue(9173);
		Value *v2 = OI.getValue(9257);
		errs() << "may: " << SC.may_equal(v1, v2) << "\n";
	}

	void Iterate::test4(Module &M) {
		ObjectID &OI = getAnalysis<ObjectID>();
		SolveConstraints &SC = getAnalysis<SolveConstraints>();
		const Use *v1 = &OI.getInstruction(4)->getOperandUse(1);
		// const Value *v1 = OI.getValue(10);
		const Type *int_type = IntegerType::get(getGlobalContext(), 32);
		const Value *v2 = ConstantInt::get(int_type, 5);
		assert(v1 && v2);
		const Clause *c = new Clause(new BoolExpr(
					CmpInst::ICMP_SLT,
					new Expr(v1),
					new Expr(v2)));
		errs() << "must: " << SC.provable(vector<const Clause *>(1, c)) << "\n";
		delete c;
	}

	void Iterate::run_tests(Module &M) {
		// test1(M);
		// test2(M);
		// test3(M);
		test4(M);
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
