/**
 * Author: Jingyue
 *
 * Test only. Not a part of int-constraints. 
 */

#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/DominatorInternals.h"
#include "llvm/Support/CommandLine.h"
#include "common/include/util.h"
#include "idm/id.h"
using namespace llvm;

#include "int-constraints/iterate.h"
#include "int-constraints/solve.h"
#include "../include/test-banner.h"
using namespace slicer;

#include <map>
#include <vector>
using namespace std;

namespace slicer {

	struct IntTest: public ModulePass {

		static char ID;

		IntTest(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;

	private:
		/* These test functions give assertion failures on incorrect results. */
		void test_aget_nocrit_slice(Module &M);
		void test_aget_nocrit_simple(Module &M);
	};
}

static RegisterPass<IntTest> X(
		"int-test",
		"Test the integer constraint solver",
		false,
		false);

static cl::opt<string> Program(
		"prog",
		cl::desc("The program being tested (e.g. aget). "
			"Usually it's simply the name of the bc file without \".bc\"."),
		cl::init(""));

char IntTest::ID = 0;

void IntTest::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<ObjectID>();
	AU.addRequired<Iterate>();
	AU.addRequired<SolveConstraints>();
	ModulePass::getAnalysisUsage(AU);
}

bool IntTest::runOnModule(Module &M) {
	if (Program == "") {
		errs() << "[Warning] You didn't specify the program name. "
			"No testcases will be executed.\n";
	}
	/*
	 * Run all test cases. 
	 * Each test case will check whether it should be run according
	 * to the program name. 
	 */
	test_aget_nocrit_slice(M);
	test_aget_nocrit_simple(M);
	return false;
}

void IntTest::test_aget_nocrit_slice(Module &M) {
	
	if (Program != "aget-nocrit.slice")
		return;
	TestBanner("aget-nocrit.slice");
}

void IntTest::test_aget_nocrit_simple(Module &M) {

	if (Program != "aget-nocrit.simple")
		return;
	TestBanner("aget-nocrit.simple");

	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	ObjectID &OI = getAnalysis<ObjectID>();
	Value *offset = OI.getValue(2751);
	Value *soffset = OI.getValue(2717);
	Value *foffset = OI.getValue(2646);
	Value *dr = OI.getValue(2750);
	Use *use_dr = &OI.getInstruction(1935)->getOperandUse(3);
	Value *remain = OI.getValue(2757); // foffset - offset
	assert(offset && soffset && foffset && dr && remain && use_dr->get() == dr);

	// soffset <= offset
	assert(SC.provable(CmpInst::ICMP_SLE, soffset, offset));
	// offset < foffset
	assert(SC.provable(CmpInst::ICMP_SLT, offset, foffset));
	// offset + dr <= foffset
	Expr *end = new Expr(Instruction::Add, new Expr(offset), new Expr(use_dr));
	Clause *c = new Clause(new BoolExpr(
				CmpInst::ICMP_SLE, end, new Expr(foffset)));
	assert(SC.provable(c));
	delete end;
	delete c;
	// offset + remain <= foffset
	end = new Expr(Instruction::Add, new Expr(offset), new Expr(remain));
	c = new Clause(new BoolExpr(CmpInst::ICMP_SLE, end, new Expr(foffset)));
	assert(SC.provable(c));
	delete end;
	delete c;
}
