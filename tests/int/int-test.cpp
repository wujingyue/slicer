/**
 * Author: Jingyue
 */

/**
 * Identify the variables only? 
 * Saves the solving time during debugging. 
 */

#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/DominatorInternals.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "common/include/util.h"
#include "common/cfg/exec-once.h"
using namespace llvm;

#include "int/iterate.h"
#include "int/capture.h"
#include "int/solve.h"
#include "int/adv-alias.h"
#include "../include/test-utils.h"
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
		void test_aget_nocrit_simple(const Module &M);
		void test_test_overwrite_simple(const Module &M);
		void test_fft_nocrit_simple(const Module &M);
		void test_fft_like_simple(const Module &M);
		void test_fft_nocrit_common(const Module &M);
		void test_radix_nocrit_simple(const Module &M);
		void test_radix_nocrit_common(const Module &M);
		void test_test_loop_simple(const Module &M);
		void test_test_reducer_simple(const Module &M);
		void test_test_bound_simple(const Module &M);
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
	AU.addRequired<ObjectID>();
	AU.addRequired<ExecOnce>();
	AU.addRequired<Iterate>();
	AU.addRequired<SolveConstraints>();
	AU.addRequired<CaptureConstraints>();
	AU.addRequired<AdvancedAlias>();
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
	test_aget_nocrit_simple(M);
	test_test_overwrite_simple(M);
	test_fft_nocrit_simple(M);
	test_fft_like_simple(M);
	test_radix_nocrit_simple(M);
	test_test_loop_simple(M);
	test_test_reducer_simple(M);
	test_test_bound_simple(M);
	return false;
}

static bool starts_with(const string &a, const string &b) {
	return a.length() >= b.length() && a.compare(0, b.length(), b) == 0;
}

void IntTest::test_test_bound_simple(const Module &M) {

	if (Program != "test-bound.simple")
		return;
	TestBanner X("test-bound.simple");

	forallconst(Module, f, M) {
		if (f->getName() != "main")
			continue;
		const GetElementPtrInst *gep1 = NULL, *gep2 = NULL;
		forallconst(Function, bb, *f) {
			forallconst(BasicBlock, ins, *bb) {
				if (const GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(ins)) {
					// Look at GEPs from <arr> only. 
					if (gep->getOperand(0)->getName() != "arr")
						continue;
					if (!gep1)
						gep1 = gep;
					else if (!gep2)
						gep2 = gep;
					else
						assert(false && "Found more than 2 GEPs");
				}
			}
		}
		assert(gep1 && gep2 && "Found less than 2 GEPs");

		SolveConstraints &SC = getAnalysis<SolveConstraints>();
		AdvancedAlias &AAA = getAnalysis<AdvancedAlias>();

		errs() << "gep1:" << *gep1 << "\ngep2:" << *gep2 << "\n";
		assert(gep1->getNumOperands() == 3 && gep2->getNumOperands() == 3);
		errs() << "i1 != i2? ...";
		assert(SC.provable(CmpInst::ICMP_NE,
					&gep1->getOperandUse(2), &gep2->getOperandUse(2)));
		print_pass(errs());
		errs() << "gep1 and gep2 alias? ...";
		assert(AAA.alias(gep1, 0, gep2, 0) == AliasAnalysis::NoAlias);
		print_pass(errs());
	}
}

void IntTest::test_test_reducer_simple(const Module &M) {

	if (Program != "test-reducer.simple")
		return;
	TestBanner X("test-reducer.simple");

	forallconst(Module, f, M) {
		if (f->getName() != "main")
			continue;
		forallconst(Function, bb, *f) {
			forallconst(BasicBlock, ins, *bb) {
				if (const CallInst *ci = dyn_cast<CallInst>(ins)) {
					const Function *callee = ci->getCalledFunction();
					if (callee && callee->getName() == "printf") {
						BasicBlock::const_iterator gep = ins;
						for (gep = bb->begin(); gep != ins; ++gep) {
							if (isa<GetElementPtrInst>(gep))
								break;
						}
						assert(isa<GetElementPtrInst>(gep) &&
								"Cannot find a GEP before the printf");
						SolveConstraints &SC = getAnalysis<SolveConstraints>();
						errs() << "GEP:" << *gep << "\n";
						const IntegerType *int_type = IntegerType::get(M.getContext(), 32);
						errs() << "argc - 1 >= 0? ...";
						assert(SC.provable(
									CmpInst::ICMP_SGT,
									&gep->getOperandUse(1),
									ConstantInt::get(int_type, 0)));
						print_pass(errs());
					}
				}
			}
		}
	}
}

void IntTest::test_test_loop_simple(const Module &M) {
	
	if (Program != "test-loop.simple")
		return;
	TestBanner X("test-loop.simple");

	unsigned n_printfs = 0;
	forallconst(Module, f, M) {
		forallconst(Function, bb, *f) {
			forallconst(BasicBlock, ins, *bb) {
				if (const CallInst *ci = dyn_cast<CallInst>(ins)) {
					const Function *callee = ci->getCalledFunction();
					if (callee && callee->getName() == "printf")
						++n_printfs;
				}
			}
		}
	}
	errs() << "3 printf's? ...";
	assert(n_printfs == 3);
	print_pass(errs());
}

void IntTest::test_radix_nocrit_simple(const Module &M) {
	
	if (Program != "RADIX-nocrit.simple")
		return;
	TestBanner X("RADIX-nocrit.simple");

	test_radix_nocrit_common(M);

	// rank_me_mynum and rank_ff_mynum
	vector<const Value *> ranks;
	forallconst(Module, f, M) {
		forallconst(Function, bb, *f) {
			forallconst(BasicBlock, ins, *bb) {
				if (ins->getOpcode() == Instruction::AShr) {
					BasicBlock::const_iterator next = ins;
					++next;
					assert(isa<GetElementPtrInst>(next));
					const GetElementPtrInst *gep = cast<GetElementPtrInst>(next);
					ranks.push_back(gep->getOperand(0));
				}
			}
		}
	}
	for (size_t i = 0; i < ranks.size(); ++i)
		errs() << "rank " << i << ":" << *ranks[i] << "\n";

	// SolveConstraints &SC = getAnalysis<SolveConstraints>();
	AdvancedAlias &AA = getAnalysis<AdvancedAlias>();
	// ObjectID &OI = getAnalysis<ObjectID>();
	for (size_t i = 0; i < ranks.size(); ++i) {
		for (size_t j = i + 1; j < ranks.size(); ++j) {
			errs() << "Comparing ranks[" << i << "] and ranks[" << j << "]... ";
			assert(AA.alias(ranks[i], 0, ranks[j], 0) == AliasAnalysis::NoAlias);
			print_pass(errs());
		}
	}
}

void IntTest::test_radix_nocrit_common(const Module &M) {

	// MyNum's are distinct. 
	vector<const Value *> local_ids;
	forallconst(Module, f, M) {

		if (starts_with(f->getName(), "slave_sort.SLICER")) {
			
			string str_id = f->getName().substr(strlen("slave_sort.SLICER"));
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
				if (ins->getOpcode() == Instruction::Store) {
					local_id = ins->getOperand(0);
					break;
				}
			}
			assert(local_id && "Cannot find the Store instruction");
			errs() << "local_id in " << f->getName() << ": " << *local_id << "\n";
			local_ids.push_back(local_id);
		}
	}

	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	for (size_t i = 0; i < local_ids.size(); ++i) {
		for (size_t j = i + 1; j < local_ids.size(); ++j) {
			errs() << "local_ids[" << i << "] != local_ids[" << j << "]? ...";
			assert(SC.provable(CmpInst::ICMP_NE, local_ids[i], local_ids[j]));
			print_pass(errs());
		}
	}

}

void IntTest::test_fft_nocrit_simple(const Module &M) {
	
	if (Program != "FFT-nocrit.simple")
		return;
	TestBanner X("FFT-nocrit.simple");

	test_fft_nocrit_common(M);
}

void IntTest::test_fft_like_simple(const Module &M) {

	if (Program != "FFT-like.simple")
		return;
	TestBanner X("FFT-like.simple");

	test_fft_nocrit_common(M);
}

void IntTest::test_fft_nocrit_common(const Module &M) {

	// MyNum's are distinct. 
	vector<const Value *> local_ids;
	forallconst(Module, f, M) {

		if (getAnalysis<ExecOnce>().not_executed(f))
			continue;
		if (!starts_with(f->getName(), "SlaveStart.SLICER"))
			continue;

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
			if (isa<StoreInst>(ins)) {
				local_id = ins->getOperand(0);
				break;
			}
		}
		assert(local_id && "Cannot find the StoreInst.");
		errs() << "local_id in " << f->getName() << ": " << *local_id << "\n";
		local_ids.push_back(local_id);
	}

	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	for (size_t i = 0; i < local_ids.size(); ++i) {
		for (size_t j = i + 1; j < local_ids.size(); ++j) {
			errs() << "local_ids[" << i << "] != local_ids[" << j << "]? ...";
			assert(SC.provable(CmpInst::ICMP_NE, local_ids[i], local_ids[j]));
			print_pass(errs());
		}
	}

	// The ranges passed to function FFT1D are disjoint. 
	vector<ConstValuePair> ranges;
	forallconst(Module, f, M) {
		if (starts_with(f->getName(), "FFT1D.SLICER")) {
			const Value *first = NULL, *last = NULL;
			for (Function::const_arg_iterator ai = f->arg_begin();
					ai != f->arg_end(); ++ai) {
				if (ai->getName() == "MyFirst")
					first = ai;
				if (ai->getName() == "MyLast")
					last = ai;
			}
			assert(first && last && "Cannot find arguments called MyFirst and MyLast");
			ranges.push_back(make_pair(first, last));
		}
	}
	for (size_t i = 0; i < ranges.size(); ++i) {
		for (size_t j = i + 1; j < ranges.size(); ++j) {
			Clause *c1 = new Clause(new BoolExpr(
						CmpInst::ICMP_SLE,
						new Expr(ranges[i].second),
						new Expr(ranges[j].first)));
			Clause *c2 = new Clause(new BoolExpr(
						CmpInst::ICMP_SLE,
						new Expr(ranges[j].second),
						new Expr(ranges[i].first)));
			Clause *disjoint = new Clause(Instruction::Or, c1, c2);
			errs() << "ranges[" << i << "] and ranges[" << j << "] are disjoint? ...";
			assert(SC.provable(disjoint));
			print_pass(errs());
			delete disjoint;
		}
	}
}

void IntTest::test_test_overwrite_simple(const Module &M) {
	
	if (Program != "test-overwrite.simple")
		return;
	TestBanner X("test-overwrite.simple");

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
	errs() << "v1:" << *v1 << "\n" << "v2:" << *v2 << "\n";
	errs() << "v1 == v2? ...";
	assert(SC.provable(CmpInst::ICMP_EQ, v1, v2));
	print_pass(errs());
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

	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	// soffset <= offset
	errs() << "soffset <= offset? ...";
	assert(SC.provable(CmpInst::ICMP_SLE, soffset, offset));
	print_pass(errs());
	
	// offset < foffset
	errs() << "offset < foffset? ...";
	assert(SC.provable(CmpInst::ICMP_SLT, offset, foffset));
	print_pass(errs());
	
	// offset + dr <= foffset
	errs() << "offset + dr <= foffset? ...";
	Expr *end = new Expr(Instruction::Add, new Expr(offset), new Expr(dr));
	Clause *c = new Clause(new BoolExpr(
				CmpInst::ICMP_SLE, end, new Expr(foffset)));
	assert(SC.provable(c));
	delete end;
	delete c;
	print_pass(errs());
	
	// offset + remain <= foffset
	errs() << "offset + remain <= foffset? ...";
	end = new Expr(Instruction::Add, new Expr(offset), new Expr(remain));
	c = new Clause(new BoolExpr(CmpInst::ICMP_SLE, end, new Expr(foffset)));
	assert(SC.provable(c));
	delete end;
	delete c;
	print_pass(errs());
	
	// foffset <= soffset2
	errs() << "foffset <= soffset2? ...";
	assert(SC.provable(CmpInst::ICMP_SLE, foffset, soffset2));
	print_pass(errs());
}
