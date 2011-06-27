/**
 * Author: Jingyue
 *
 * Test only. Not a part of int-constraints. 
 */

/**
 * Identify the variables only? 
 * Saves the solving time during debugging. 
 */
#define IDENTIFY_ONLY

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
		void test_aget_nocrit_slice(const Module &M);
		void test_aget_nocrit_simple(const Module &M);
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
#ifndef IDENTIFY_ONLY
	AU.addRequired<Iterate>();
	AU.addRequired<SolveConstraints>();
#endif
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

void IntTest::test_aget_nocrit_slice(const Module &M) {
	
	if (Program != "aget-nocrit.slice")
		return;
	TestBanner("aget-nocrit.slice");
}

void IntTest::test_aget_nocrit_simple(const Module &M) {

	if (Program != "aget-nocrit.simple")
		return;
	TestBanner X("aget-nocrit.simple");

	/*
	 * Instruction IDs and value IDs are not deterministic because max-slicing
	 * is not. Need a way to identify instructions and values more
	 * deterministically regardless of the ID mapping. 
	 */
	const Function *http_get_slicer = NULL;
	forallconst(Module, fi, M) {
		if (fi->getNameStr() == "http_get.SLICER") {
			http_get_slicer = fi;
			break;
		}
	}
	assert(http_get_slicer && "Cannot find function http_get.SLICER");

	unsigned n_pwrites = 0;
	const Value *offset = NULL, *soffset = NULL, *foffset = NULL, *remain = NULL;
	const Use *dr = NULL;
	forallconst(Function, bi, *http_get_slicer) {
		forallconst(BasicBlock, ii, *bi) {
			if (const CallInst *ci = dyn_cast<CallInst>(ii)) {
				const Function *callee = ci->getCalledFunction();
				if (callee && callee->getNameStr() == "pwrite") {
					
					const Use *off_use = &ci->getOperandUse(4);
					const Use *len_use = &ci->getOperandUse(3);
					const Value *off = off_use->get();
					const Value *len = len_use->get();

					const LoadInst *li = dyn_cast<LoadInst>(off);
					assert(li);
					const GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(
							li->getPointerOperand());
					assert(gep);
					assert(2 < gep->getNumOperands());
					const ConstantInt *idx = dyn_cast<ConstantInt>(gep->getOperand(2));
					if (idx->getZExtValue() == 4) {
						offset = off;
						if (isa<CallInst>(len))
							dr = len_use;
						else
							remain = len;
					} else if (idx->getZExtValue() == 2) {
						soffset = off;
					}
					++n_pwrites;
				}
			} // if CallInst
			if (const LoadInst *li = dyn_cast<LoadInst>(ii)) {
				assert(http_get_slicer->getArgumentList().size() == 1);
				const Value *arg = http_get_slicer->getArgumentList().begin();
				const GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(
						li->getPointerOperand());
				if (gep && 2 < gep->getNumOperands() && gep->getOperand(0) == arg) {
					bool found = true;
					if (const ConstantInt *idx_1 = dyn_cast<ConstantInt>(
								gep->getOperand(1))) {
						if (idx_1->getZExtValue() != 0)
							found = false;
					}
					if (const ConstantInt *idx_2 = dyn_cast<ConstantInt>(
								gep->getOperand(2))) {
						if (idx_2->getZExtValue() != 3)
							found = false;
					}
					if (found)
						foffset = ii;
				}
			}
		}
	}
	assert(n_pwrites == 4 && "There should be exactly 4 pwrites "
			"in function http_get.SLICER");
	assert(soffset && foffset && offset && remain && dr);
	errs() << "offset:" << *offset << "\n";
	errs() << "soffset:" << *soffset << "\n";
	errs() << "foffset:" << *foffset << "\n";
	errs() << "dr:" << *(dr->get()) << "\n";
	errs() << "remain:" << *remain << "\n";

#ifndef IDENTIFY_ONLY
	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	// soffset <= offset
	assert(SC.provable(CmpInst::ICMP_SLE, soffset, offset));
	// offset < foffset
	assert(SC.provable(CmpInst::ICMP_SLT, offset, foffset));
	// offset + dr <= foffset
	Expr *end = new Expr(Instruction::Add, new Expr(offset), new Expr(dr));
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
#endif
}
