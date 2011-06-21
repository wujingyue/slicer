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
		/*
		 * # of constraints is not a good indicator to decide whether we can
		 * stop the iterating process. 
		 * It may increase, decrease or unchange after an iteration. 
		 */
		long fingerprint;
		do {
			fingerprint = CC.get_fingerprint();
			errs() << "Iterating... # of constraints = "
				<< CC.get_num_constraints() << "\n";
			errs() << "AAA cache size = " << AAA.get_cache_size() << "\n";
			AAA.runOnModule(M); // Essentially clear the cache. 
			CC.runOnModule(M);
			SC.runOnModule(M);
		} while (CC.get_fingerprint() != fingerprint);

		if (RunTest)
			run_tests(M);

		return false;
	}

	void Iterate::test1(Module &M) {
		errs() << "===== test1 =====\n";
		ObjectID &OI = getAnalysis<ObjectID>();
		SolveConstraints &SC = getAnalysis<SolveConstraints>();
		const IntegerType *int_type = IntegerType::get(getGlobalContext(), 32);
		const Value *v1 = OI.getValue(3392);
		const Value *v2 = ConstantInt::get(int_type, 2);
		errs() << SC.must_equal(v1, v2) << "\n";
	}

	void Iterate::test2(Module &M) {
		ObjectID &OI = getAnalysis<ObjectID>();
		SolveConstraints &SC = getAnalysis<SolveConstraints>();
		Value *off1 = OI.getValue(9175);
		Value *len1 = OI.getValue(9179);
		Value *off2 = OI.getValue(17126);
		// Value *len2 = OI.getValue(17130);
		const Clause *c = new Clause(new BoolExpr(
					CmpInst::ICMP_SLE,
					new Expr(Instruction::Add, new Expr(off1), new Expr(len1)),
					new Expr(off2)));
		errs() << "must: " << SC.provable(vector<const Clause *>(1, c)) << "\n";
		delete c;
	}

	void Iterate::test3(Module &M) {
		ObjectID &OI = getAnalysis<ObjectID>();
		SolveConstraints &SC = getAnalysis<SolveConstraints>();
		Value *v1 = OI.getValue(3289);
		Value *v2 = OI.getValue(3358);
		const Clause *c = new Clause(new BoolExpr(
					CmpInst::ICMP_SLE, new Expr(v1), new Expr(v2)));
		errs() << "v1 <= v2: " << SC.provable(vector<const Clause *>(1, c)) << "\n";
		delete c;
	}

	void Iterate::test4(Module &M) {
		ObjectID &OI = getAnalysis<ObjectID>();
		SolveConstraints &SC = getAnalysis<SolveConstraints>();
		// const Value *v1 = OI.getInstruction(2)->getOperand(0);
		// const Use *v1 = &OI.getInstruction(2)->getOperandUse(0);
		const Instruction *v1 = OI.getInstruction(2);
		const IntegerType *int_type = IntegerType::get(getGlobalContext(), 32);
		const Value *v2 = ConstantInt::get(int_type, 5);
		assert(v1 && v2);
		const Clause *c = new Clause(new BoolExpr(
					CmpInst::ICMP_SLT,
					new Expr(v1),
					new Expr(v2)));
		errs() << "must: " << SC.provable(vector<const Clause *>(1, c)) << "\n";
		delete c;
	}

	void Iterate::test5(Module &M) {
		CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
		CC.print(outs(), &M);
	}

	void Iterate::run_tests(Module &M) {
		test1(M);
		// test2(M);
		// test3(M);
		// test4(M);
		// test5(M);
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
