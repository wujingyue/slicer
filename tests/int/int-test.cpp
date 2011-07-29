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
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "common/include/util.h"
#include "common/cfg/exec-once.h"
using namespace llvm;

#include "int/iterate.h"
#include "int/capture.h"
#include "int/solve.h"
#include "int/adv-alias.h"
#include "max-slicing/region-manager.h"
#include "tests/include/test-utils.h"
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
		void test_aget_like_simple(const Module &M);
		void test_test_overwrite_simple(const Module &M);
		void test_fft_nocrit_simple(const Module &M);
		void test_fft_like_simple(const Module &M);
		void test_fft_nocrit_common(const Module &M);
		void test_radix_nocrit_simple(const Module &M);
		void test_radix_nocrit_common(const Module &M);
		void test_test_loop_simple(const Module &M);
		void test_test_reducer_simple(const Module &M);
		void test_test_bound_simple(const Module &M);
		void test_test_thread_simple(const Module &M);
		void test_test_array_simple(const Module &M);
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
	AU.addRequired<ExecOnce>();
	AU.addRequired<RegionManager>();
#ifndef IDENTIFY_ONLY
	AU.addRequired<Iterate>();
	AU.addRequired<SolveConstraints>();
	AU.addRequired<CaptureConstraints>();
	AU.addRequired<AdvancedAlias>();
#endif
	ModulePass::getAnalysisUsage(AU);
}

bool IntTest::runOnModule(Module &M) {
	if (Program == "") {
		CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
		CC.print(errs(), &M);
#if 0
		SolveConstraints &SC = getAnalysis<SolveConstraints>();
		SC.print_assertions();
#endif
		return false;
	}
	/*
	 * Run all test cases. 
	 * Each test case will check whether it should be run according
	 * to the program name. 
	 */
	test_aget_nocrit_simple(M);
	test_aget_like_simple(M);
	test_test_overwrite_simple(M);
	test_fft_nocrit_simple(M);
	test_fft_like_simple(M);
	test_radix_nocrit_simple(M);
	test_test_loop_simple(M);
	test_test_reducer_simple(M);
	test_test_bound_simple(M);
	test_test_thread_simple(M);
	test_test_array_simple(M);
	return false;
}

static bool starts_with(const string &a, const string &b) {
	return a.length() >= b.length() && a.compare(0, b.length(), b) == 0;
}

void IntTest::test_test_array_simple(const Module &M) {

	if (Program != "test-array.simple")
		return;
	TestBanner X("test-array.simple");

	for (Module::const_global_iterator gi = M.global_begin();
			gi != M.global_end(); ++gi) {
		if (gi->getName() == "global_arr") {
			errs() << "Found global_arr\n";
			assert(gi->hasInitializer());
			assert(isa<ConstantAggregateZero>(gi->getInitializer()));
		}
	}
}

void IntTest::test_test_thread_simple(const Module &M) {

	if (Program != "test-thread.simple")
		return;
	TestBanner X("test-thread.simple");

	vector<const Value *> local_ids;
	forallconst(Module, f, M) {
		if (starts_with(f->getName(), "sub_routine.SLICER")) {
			assert(f->arg_size() == 1);
			local_ids.push_back(f->arg_begin());
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
	
	// The ranges passed to Transpose are disjoint. 
	DenseMap<const Function *, vector<ConstValuePair> > function_ranges;
	forallconst(Module, f, M) {
		forallconst(Function, bb, *f) {
			forallconst(BasicBlock, ins, *bb) {
				CallSite cs = CallSite::get(
						const_cast<Instruction *>((const Instruction *)ins));
				if (!cs.getInstruction())
					continue;
				const Function *callee = cs.getCalledFunction();
				if (callee && callee->getName() == "Transpose") {
					assert(cs.arg_size() == 7);
					dbgs() << f->getName() << ":" << *ins << "\n";
					function_ranges[f].push_back(make_pair(
								cs.getArgument(4), cs.getArgument(5)));
				}
			}
		}
	}

	DenseMap<const Function *, vector<ConstValuePair> >::iterator i1, i2;
	for (i1 = function_ranges.begin(); i1 != function_ranges.end(); ++i1) {
		for (i2 = i1, ++i2; i2 != function_ranges.end(); ++i2) {
			for (size_t j1 = 0; j1 < i1->second.size(); ++j1) {
				for (size_t j2 = 0; j2 < i2->second.size(); ++j2) {
					Clause *c1 = new Clause(new BoolExpr(CmpInst::ICMP_SLE,
								new Expr(i1->second[j1].second),
								new Expr(i2->second[j2].first)));
					Clause *c2 = new Clause(new BoolExpr(CmpInst::ICMP_SLE,
								new Expr(i2->second[j2].second),
								new Expr(i1->second[j1].first)));
					Clause *disjoint = new Clause(Instruction::Or, c1, c2);
					errs() << "{" << i1->first->getName() << ":" << j1 << "} and {" <<
						i2->first->getName() << ":" << j2 << "} are disjoint? ...";
					assert(SC.provable(disjoint));
					print_pass(errs());
					delete disjoint;
				}
			}
		}
	}
}

void IntTest::test_aget_like_simple(const Module &M) {

	if (Program != "aget-like.simple")
		return;
	TestBanner X("aget-like.simple");

	vector<vector<ConstUsePair> > ranges;
	forallconst(Module, f, M) {
		
		if (!starts_with(f->getName(), "http_get.SLICER"))
			continue;
		errs() << "=== Function " << f->getName() << " ===\n";
		
		ranges.push_back(vector<ConstUsePair>());
		forallconst(Function, bb, *f) {
			forallconst(BasicBlock, ins, *bb) {
				if (const CallInst *ci = dyn_cast<CallInst>(ins)) {
					const Function *callee = ci->getCalledFunction();
					if (callee && callee->getName() == "fake_write") {
						// fake_write(buffer, size, offset)
						errs() << *ci << "\n";
						ranges.back().push_back(make_pair(
									&ci->getOperandUse(3),
									&ci->getOperandUse(2)));
					}
				}
			}
		}
	}

	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	for (size_t i1 = 0; i1 < ranges.size(); ++i1) {
		for (size_t i2 = i1 + 1; i2 < ranges.size(); ++i2) {
			for (size_t j1 = 0; j1 < ranges[i1].size(); ++j1) {
				for (size_t j2 = 0; j2 < ranges[i2].size(); ++j2) {
					Expr *end1 = new Expr(Instruction::Add,
							new Expr(ranges[i1][j1].first), new Expr(ranges[i1][j1].second));
					Expr *end2 = new Expr(Instruction::Add,
							new Expr(ranges[i2][j2].first), new Expr(ranges[i2][j2].second));
					// end1 <= start2 or end2 <= start1
					Clause *c1 = new Clause(new BoolExpr(CmpInst::ICMP_SLE,
								end1, new Expr(ranges[i2][j2].first)));
					Clause *c2 = new Clause(new BoolExpr(CmpInst::ICMP_SLE,
								end2, new Expr(ranges[i1][j1].first)));
					errs() << "{" << i1 << ", " << j1 << "} and {" << i2 << ", " << j2 <<
						"} are disjoint? ...";
					assert(SC.provable(new Clause(Instruction::Or, c1, c2)));
					print_pass(errs());
				}
			}
		}
	}
}

void IntTest::test_aget_nocrit_simple(const Module &M) {

	if (Program != "aget-nocrit.simple")
		return;
	TestBanner X("aget-nocrit.simple");

	vector<const Function *> thr_funcs;
	forallconst(Module, f, M) {
		if (starts_with(f->getName(), "http_get.SLICER"))
			thr_funcs.push_back(f);
	}
	dbgs() << "Thread functions:";
	for (size_t i = 0; i < thr_funcs.size(); ++i)
		dbgs() << " " << thr_funcs[i]->getName();
	dbgs() << "\n";
	
	vector<const Value *> soffsets(thr_funcs.size(), NULL);
	vector<const Value *> foffsets(thr_funcs.size(), NULL);
	for (size_t i = 0; i < thr_funcs.size(); ++i) {
		const Function *f = thr_funcs[i];
		assert(distance(f->arg_begin(), f->arg_end()) == 1);
		const Value *td = f->arg_begin();
		for (Value::use_const_iterator ui = td->use_begin();
				ui != td->use_end(); ++ui) {
			if (const GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(*ui)) {
				if (gep->getNumOperands() >= 3 && gep->getOperand(0) == td) {
					const ConstantInt *op1 = dyn_cast<ConstantInt>(gep->getOperand(1));
					const ConstantInt *op2 = dyn_cast<ConstantInt>(gep->getOperand(2));
					if (op1 && op2 && op1->isZero()) {
						uint64_t index = op2->getZExtValue();
						// struct thread_data
						// 2: soffset
						// 3: foffset
						// 4: offset
						if (index == 2 && soffsets[i] == NULL) {
							for (Value::use_const_iterator ui2 = gep->use_begin();
									ui2 != gep->use_end(); ++ui2) {
								if (isa<LoadInst>(*ui2)) {
									soffsets[i] = *ui2;
									break;
								}
							}
						}
						if (index == 3 && foffsets[i] == NULL) {
							for (Value::use_const_iterator ui2 = gep->use_begin();
									ui2 != gep->use_end(); ++ui2) {
								if (isa<LoadInst>(*ui2)) {
									foffsets[i] = *ui2;
									break;
								}
							}
						}
					}
				}
			}
		}
	}
	for (size_t i = 0; i < thr_funcs.size(); ++i) {
		dbgs() << "soffset in thread " << i << ":";
		if (soffsets[i])
			dbgs() << *soffsets[i] << "\n";
		else
			dbgs() << "  <null>\n";
		dbgs() << "foffset in thread " << i << ":";
		if (foffsets[i])
			dbgs() << *foffsets[i] << "\n";
		else
			dbgs() << "  <null>\n";
	}

	vector<vector<ConstUsePair> > ranges;
	ranges.resize(thr_funcs.size());
	for (size_t i = 0; i < thr_funcs.size(); ++i) {
		const Function *f = thr_funcs[i];
		forallconst(Function, bb, *f) {
			forallconst(BasicBlock, ins, *bb) {
				if (const CallInst *ci = dyn_cast<CallInst>(ins)) {
					const Function *callee = ci->getCalledFunction();
					if (callee && callee->getName() == "pwrite") {
						assert(ci->getNumOperands() == 5);
						// pwrite(???, ???, len, offset)
						const Value *offset = ci->getOperand(4);
						if (const LoadInst *li = dyn_cast<LoadInst>(offset)) {
							const GetElementPtrInst *gep =
								dyn_cast<GetElementPtrInst>(li->getPointerOperand());
							assert(gep);
							assert(gep->getNumOperands() > 2);
							const ConstantInt *idx = dyn_cast<ConstantInt>(gep->getOperand(2));
							if (idx->getZExtValue() == 4) {
								// From <offset> rather than <soffset>. 
								ranges[i].push_back(make_pair(
											&ci->getOperandUse(4), &ci->getOperandUse(3)));
							}
						}
					}
				}
			}
		}
		dbgs() << "Ranges in thread " << i << ":\n";
		for (size_t j = 0; j < ranges[i].size(); ++j) {
			const User *user = ranges[i][j].first->getUser();
			assert(user == ranges[i][j].second->getUser());
			dbgs() << *user << "\n";
		}
	}
	

	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	for (size_t i = 0; i < thr_funcs.size(); ++i) {
		if (soffsets[i] && foffsets[i]) {
			errs() << "soffsets[" << i << "] <= foffsets[" << i << "]? ...";
			assert(SC.provable(CmpInst::ICMP_SLE, soffsets[i], foffsets[i]));
			print_pass(errs());
		}
	}
	
	for (size_t i = 0; i < thr_funcs.size(); ++i) {
		for (size_t j = 0; j < ranges[i].size(); ++j) {
			errs() << "Range {" << i << ", " << j << "}\n";
			if (soffsets[i]) {
				errs() << "  soffsets <= offset? ...";
				assert(SC.provable(CmpInst::ICMP_SLE, soffsets[i], ranges[i][j].first));
				print_pass(errs());
			}
			if (foffsets[i]) {
				errs() << "  offset < foffset? ...";
				assert(SC.provable(CmpInst::ICMP_SLT, ranges[i][j].first, foffsets[i]));
				print_pass(errs());
				errs() << "  offset + len <= foffset? ...";
				assert(SC.provable(new Clause(new BoolExpr(CmpInst::ICMP_SLE,
									new Expr(Instruction::Add,
										new Expr(ranges[i][j].first), new Expr(ranges[i][j].second)),
									new Expr(foffsets[i])))));
				print_pass(errs());
			}
		}
	}
	
	for (size_t i1 = 0; i1 < thr_funcs.size(); ++i1) {
		for (size_t i2 = i1 + 1; i2 < thr_funcs.size(); ++i2) {
			for (size_t j1 = 0; j1 < ranges[i1].size(); ++j1) {
				for (size_t j2 = 0; j2 < ranges[i2].size(); ++j2) {
					Expr *end1 = new Expr(Instruction::Add,
							new Expr(ranges[i1][j1].first), new Expr(ranges[i1][j1].second));
					Expr *end2 = new Expr(Instruction::Add,
							new Expr(ranges[i2][j2].first), new Expr(ranges[i2][j2].second));
					// end1 <= start2 or end2 <= start1
					Clause *c1 = new Clause(new BoolExpr(CmpInst::ICMP_SLE,
								end1, new Expr(ranges[i2][j2].first)));
					Clause *c2 = new Clause(new BoolExpr(CmpInst::ICMP_SLE,
								end2, new Expr(ranges[i1][j1].first)));
					errs() << "{" << i1 << ", " << j1 << "} and {" << i2 << ", " << j2 <<
						"} are disjoint? ...";
					assert(SC.provable(new Clause(Instruction::Or, c1, c2)));
					print_pass(errs());
				}
			}
		}
	}
}
