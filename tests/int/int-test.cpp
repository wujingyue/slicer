/**
 * Author: Jingyue
 */

/**
 * Identify the variables only? 
 * Saves the solving time during debugging. 
 */
// #define IDENTIFY_ONLY

#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/DominatorInternals.h"
#include "llvm/Support/CommandLine.h"
#include "common/include/util.h"
#include "idm/id.h"
using namespace llvm;

#include "int/iterate.h"
#include "int/capture.h"
#include "int/solve.h"
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
		void test_test_overwrite_slice(const Module &M);
		void test_fft_nocrit_slice(const Module &M);
	};
}

static RegisterPass<IntTest> X(
		"int-test",
		"Test the integer constraint solver",
		false,
		false);
// If Program == "", we dump all the integer constraints. 
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
	AU.addRequired<CaptureConstraints>();
	AU.addRequired<ObjectID>();
#endif
	ModulePass::getAnalysisUsage(AU);
}

bool IntTest::runOnModule(Module &M) {
	if (Program == "") {
		CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
		CC.print(errs(), &M);
		return false;
	}
	/*
	 * Run all test cases. 
	 * Each test case will check whether it should be run according
	 * to the program name. 
	 */
	test_aget_nocrit_slice(M);
	test_aget_nocrit_simple(M);
	test_test_overwrite_slice(M);
	test_fft_nocrit_slice(M);
	return false;
}

static bool starts_with(const string &a, const string &b) {
	return a.length() >= b.length() && a.compare(0, b.length(), b) == 0;
}

void IntTest::test_fft_nocrit_slice(const Module &M) {
	
	if (Program != "FFT-nocrit.slice")
		return;
	TestBanner X("FFT-nocrit.slice");

	vector<const Value *> local_ids;
	forallconst(Module, f, M) {

		if (starts_with(f->getName(), "SlaveStart.SLICER")) {
			
			string str_id = f->getName().substr(strlen("SlaveStart.SLICER"));
			int id = (str_id == "" ? 0 : atoi(str_id.c_str()) - 1);
			assert(id >= 0);
			const Instruction *start = NULL;
			for (BasicBlock::const_iterator ins = f->getEntryBlock().begin();
					ins != f->getEntryBlock().end(); ++ins) {
				if (const CallInst *ci = dyn_cast<CallInst>(ins)) {
					const Function *callee = ci->getCalledFunction();
					if (callee && callee->getName() == "pthread_mutex_lock") {
						start = ci;
						break;
					}
				}
			}
			assert(start && "Cannot find a pthread_mutex_lock in the entry block");
			
			const Value *local_id = NULL;
			for (BasicBlock::const_iterator ins = start;
					ins != f->getEntryBlock().end(); ++ins) {
				if (ins->getOpcode() == Instruction::Add) {
					local_id = ins->getOperand(0);
					break;
				}
			}
			assert(local_id && "Cannot find the Add instruction");
			errs() << "local_id in " << f->getName() << ": " << *local_id << "\n";
			local_ids.push_back(local_id);
		}
	}

	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	for (size_t i = 0; i < local_ids.size(); ++i) {
		for (size_t j = i + 1; j < local_ids.size(); ++j) {
			assert(SC.provable(CmpInst::ICMP_NE, local_ids[i], local_ids[j]));
		}
	}
}

void IntTest::test_aget_nocrit_slice(const Module &M) {
	
	if (Program != "aget-nocrit.slice")
		return;
	TestBanner X("aget-nocrit.slice");
}

void IntTest::test_test_overwrite_slice(const Module &M) {
	
	if (Program != "test-overwrite.slice")
		return;
	TestBanner X("test-overwrite.slice");

	const Value *v1 = NULL, *v2 = NULL;
	forallconst(Module, f, M) {
		forallconst(Function, bb, *f) {
			if (f->getName() == "main.OLDMAIN")
				continue;
			forallconst(BasicBlock, ins, *bb) {
				if (const LoadInst *li = dyn_cast<LoadInst>(ins)) {
					if (li->isVolatile()) {
						if (v1 == NULL)
							v1 = li;
						else if (v2 == NULL)
							v2 = li;
						else
							assert(false && "There should be exactly 2 volatile loads.");
					}
				}
			}
		}
	}
	assert(v1 && v2 && "There should be exactly 2 volatile loads.");
	
	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	errs() << *v1 << "\n" << *v2 << "\n";
	assert(SC.provable(CmpInst::ICMP_EQ, v1, v2));
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
	const Function *http_get_slicer = NULL, *http_get_slicer2 = NULL;
	forallconst(Module, fi, M) {
		if (fi->getName() == "http_get.SLICER") {
			assert(!http_get_slicer);
			http_get_slicer = fi;
		}
		if (fi->getName() == "http_get.SLICER2") {
			assert(!http_get_slicer2);
			http_get_slicer2 = fi;
		}
	}
	assert(http_get_slicer && "Cannot find function http_get.SLICER");
	assert(http_get_slicer2 && "Cannot find function http_get.SLICER2");

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

	const Value *soffset2 = NULL;
	n_pwrites = 0;
	forallconst(Function, bi, *http_get_slicer2) {
		forallconst(BasicBlock, ii, *bi) {
			if (const CallInst *ci = dyn_cast<CallInst>(ii)) {
				const Function *callee = ci->getCalledFunction();
				if (callee && callee->getNameStr() == "pwrite") {
					
					const Use *off_use = &ci->getOperandUse(4);
					const Value *off = off_use->get();

					const LoadInst *li = dyn_cast<LoadInst>(off);
					assert(li);
					const GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(
							li->getPointerOperand());
					assert(gep);
					assert(2 < gep->getNumOperands());
					const ConstantInt *idx = dyn_cast<ConstantInt>(gep->getOperand(2));
					if (idx->getZExtValue() == 2)
						soffset2 = off;
					++n_pwrites;
				}
			} // if CallInst
		}
	}
	assert(n_pwrites == 4 && "There should be exactly 4 pwrites "
			"in function http_get.SLICER2");
	assert(soffset2);
	errs() << "soffset2:" << *soffset2 << "\n";

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
	// foffset <= soffset2
	assert(SC.provable(CmpInst::ICMP_SLE, foffset, soffset2));
#endif
}
