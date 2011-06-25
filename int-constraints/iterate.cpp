#include "bc2bdd/BddAliasAnalysis.h"
using namespace repair;

#include "llvm/LLVMContext.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Timer.h"
using namespace llvm;

#include "capture.h"
#include "solve.h"
#include "adv-alias.h"
#include "iterate.h"
using namespace slicer;

#include <sstream>
using namespace std;

static RegisterPass<Iterate> X(
		"iterate",
		"A iterator to provide more accurate analyses",
		false,
		false); // Not an analysis.

static cl::opt<bool> RunTest(
		"test",
		cl::desc("Whether to run tests"));

char Iterate::ID = 0;

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
	TimerGroup tg("Iterator");
	vector<Timer *> timers;
	int iter_no = 0;
	do {
		++iter_no;
		ostringstream oss; oss << "Iteration " << iter_no;
		Timer *timer = new Timer(oss.str(), tg);
		timers.push_back(timer);
		timer->startTimer();
		fingerprint = CC.get_fingerprint();
		errs() << "=== Running iteration " << iter_no << "... ===\n";
		errs() << "# of constraints = " << CC.get_num_constraints() << "\n";
		errs() << "AAA cache size = " << AAA.get_cache_size() << "\n";
		AAA.runOnModule(M); // Essentially clear the cache. 
		CC.runOnModule(M);
		SC.runOnModule(M);
		timer->stopTimer();
	} while (CC.get_fingerprint() != fingerprint);
	for (size_t i = 0; i < timers.size(); ++i)
		delete timers[i];

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
	errs() << "===== test2 =====\n";
	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	ObjectID &OI = getAnalysis<ObjectID>();
	const IntegerType *int_type = IntegerType::get(getGlobalContext(), 32);
	const Value *zero = ConstantInt::get(int_type, 0);
	const Value *one = ConstantInt::get(int_type, 1);
	forallinst(M, ii) {
		if (BranchInst *bi = dyn_cast<BranchInst>(ii)) {
			if (bi->isUnconditional())
				continue;
			const Value *cond = bi->getCondition();
			if (!CC.is_constant(cond))
				continue;
			errs() << OI.getInstructionID(bi) << ":" << *bi;
			Clause *c;
			bool ret;
			const Use *use_cond = &bi->getOperandUse(0);
			c = new Clause(new BoolExpr(
						CmpInst::ICMP_EQ, new Expr(use_cond), new Expr(one)));
			ret = SC.provable(vector<const Clause *>(1, c));
			delete c;
			if (ret) {
				errs() << " === True\n";
				continue;
			}
			c = new Clause(new BoolExpr(
						CmpInst::ICMP_EQ, new Expr(use_cond), new Expr(zero)));
			ret = SC.provable(vector<const Clause *>(1, c));
			if (ret) {
				errs() << " === False\n";
				continue;
			}
			errs() << " === Unknown\n";
		}
	}
}

void Iterate::test3(Module &M) {
	ObjectID &OI = getAnalysis<ObjectID>();
	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	Value *v1 = OI.getValue(2646);
	Value *v2 = OI.getValue(2803);
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
	outs().flush();
}

void Iterate::run_tests(Module &M) {
	// test1(M);
	// test2(M);
	test3(M);
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
