/**
 * Author: Jingyue
 */

/**
 * Identify the variables only? 
 * Saves the solving time during debugging. 
 */
// #define IDENTIFY_ONLY

#include <map>
#include <vector>
using namespace std;

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
#include "int-test.h"
using namespace slicer;

static RegisterPass<IntTest> X("int-test",
		"Test the integer constraint solver");

// If Program == "", we dump all the integer constraints. 
static cl::opt<string> Program("prog",
		cl::desc("The program being tested (e.g. aget). "
			"Usually it's simply the name of the bc file without \".bc\"."),
		cl::init(""));

char IntTest::ID = 0;

void IntTest::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<ExecOnce>();
	AU.addRequired<RegionManager>();
	AU.addRequired<IDAssigner>();
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
	test_aget(M);
	test_aget_like(M);
	test_test_overwrite(M);
	test_test_overwrite_2(M);
	test_fft(M);
	test_fft_like(M);
	test_fft_tern(M);
	test_radix(M);
	test_radix_like(M);
	test_test_loop(M);
	test_test_loop_2(M);
	test_test_reducer(M);
	test_test_bound(M);
	test_test_thread(M);
	test_test_array(M);
	test_test_malloc(M);
	test_test_range(M);
	test_test_range_2(M);
	test_test_range_3(M);
	test_test_range_4(M);
	test_test_dep(M);
	if (Program == "test-ctxt-2")
		test_test_ctxt_2(M);
	return false;
}

static bool starts_with(const string &a, const string &b) {
	return a.length() >= b.length() && a.compare(0, b.length(), b) == 0;
}

void IntTest::test_test_ctxt_2(const Module &M) {
	TestBanner X("test-ctxt-2");

	SolveConstraints &SC = getAnalysis<SolveConstraints>();

	const Function *foo = M.getFunction("foo");
	assert(foo && "Cannot find function <foo>");
	const Value *b = NULL;
	forallconst(Function, bb, *foo) {
		forallconst(BasicBlock, ins, *bb) {
			if (ins->getOpcode() == Instruction::Add) {
				assert(!b && "Multiple variable <b>");
				b = ins;
			}
		}
	}
	assert(b && "Cannot find variable <b>");

	const IntegerType *int_type = IntegerType::get(M.getContext(), 32);
	errs() << "b == 2 or 3? ...";
	assert(SC.provable(new Clause(Instruction::Or,
					new Clause(new BoolExpr(CmpInst::ICMP_EQ,
							new Expr(b, 1), new Expr(ConstantInt::get(int_type, 2)))),
					new Clause(new BoolExpr(CmpInst::ICMP_EQ,
							new Expr(b, 1), new Expr(ConstantInt::get(int_type, 3)))))));
	print_pass(errs());
}

void IntTest::test_test_dep(const Module &M) {
	if (Program != "test-dep")
		return;
	TestBanner X("test-dep");

	test_test_dep_common(M);
}

void IntTest::test_test_range_4(const Module &M) {
	if (Program != "test-range-4")
		return;
	TestBanner X("test-range-4");

	const PHINode *phi = NULL;
	forallconst(Module, f, M) {
		forallconst(Function, bb, *f) {
			forallconst(BasicBlock, ins, *bb) {
				if (const PHINode *p = dyn_cast<PHINode>(ins)) {
					assert(!phi);
					phi = p;
				}
			}
		}
	}
	assert(phi);

	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	const IntegerType *int_type = IntegerType::get(M.getContext(), 32);
	assert(SC.provable(CmpInst::ICMP_SGE,
				dyn_cast<Value>(phi),
				dyn_cast<Value>(ConstantInt::get(int_type, 0))));
	assert(SC.provable(CmpInst::ICMP_SLE,
				dyn_cast<Value>(phi),
				dyn_cast<Value>(ConstantInt::get(int_type, 110))));
}

void IntTest::test_test_dep_common(const Module &M) {
	ExecOnce &EO = getAnalysis<ExecOnce>();

	DenseMap<const Function *, ConstValueList> accesses;
	forallconst(Module, f, M) {
		if (EO.not_executed(f))
			continue;
		if (!starts_with(f->getName(), "slave_sort.SLICER"))
			continue;
		forallconst(Function, bb, *f) {
			forallconst(BasicBlock, ins, *bb) {
				if (const StoreInst *si = dyn_cast<StoreInst>(ins)) {
					if (const ConstantInt *ci =
							dyn_cast<ConstantInt>(si->getOperand(0))) {
						if (ci->isZero()) {
							errs() << f->getName() << "." << bb->getName() << ":" <<
								*ins << "\n";
							accesses[f].push_back(si->getPointerOperand());
						}
					}
				}
			}
		}
	}
	assert(accesses.size() == 2);

	DenseMap<const Function *, ConstValueList>::iterator i1, i2;
	i1 = accesses.begin();
	i2 = i1; ++i2;

	for (size_t j1 = 0; j1 < i1->second.size(); ++j1) {
		for (size_t j2 = 0; j2 < i2->second.size(); ++j2) {
			AdvancedAlias &AA = getAnalysis<AdvancedAlias>();
			errs() << "{" << i1->first->getName() << ":" << j1<< "} and {" <<
				i2->first->getName() << ":" << j2 << "} don't alias? ...";
			assert(AA.alias(i1->second[j1], 0, i2->second[j2], 0) ==
					AliasAnalysis::NoAlias);
			print_pass(errs());
		}
	}
}

void IntTest::test_test_range_3(const Module &M) {
	
	if (Program != "test-range-3")
		return;
	TestBanner X("test-range-3");

	ExecOnce &EO = getAnalysis<ExecOnce>();
	DenseMap<const Function *, ConstValueList> accesses;
	forallconst(Module, f, M) {
		if (EO.not_executed(f))
			continue;
		if (is_main(f))
			continue;
		forallconst(Function, bb, *f) {
			forallconst(BasicBlock, ins, *bb) {
				if (const StoreInst *si = dyn_cast<StoreInst>(ins)) {
					if (si->getOperand(0)->getType()->isDoubleTy()) {
						errs() << f->getName() << "." << bb->getName() << ":" <<
							*ins << "\n";
						accesses[f].push_back(si->getPointerOperand());
					}
				}
			}
		}
	}
	assert(accesses.size() == 2);

	DenseMap<const Function *, ConstValueList>::iterator i1, i2;
	i1 = accesses.begin();
	i2 = i1; ++i2;

	for (size_t j1 = 0; j1 < i1->second.size(); ++j1) {
		for (size_t j2 = 0; j2 < i2->second.size(); ++j2) {
			AdvancedAlias &AA = getAnalysis<AdvancedAlias>();
			errs() << "{" << i1->first->getName() << ":" << j1<< "} and {" <<
				i2->first->getName() << ":" << j2 << "} don't alias? ...";
			assert(AA.alias(i1->second[j1], 0, i2->second[j2], 0) ==
					AliasAnalysis::NoAlias);
			print_pass(errs());
		}
	}
}

void IntTest::test_test_range_2(const Module &M) {
	
	if (Program != "test-range-2")
		return;
	TestBanner X("test-range-2");

	ExecOnce &EO = getAnalysis<ExecOnce>();
	vector<const Value *> accesses;
	forallconst(Module, f, M) {
		if (EO.not_executed(f))
			continue;
		if (is_main(f))
			continue;
		forallconst(Function, bb, *f) {
			forallconst(BasicBlock, ins, *bb) {
				if (const StoreInst *si = dyn_cast<StoreInst>(ins)) {
					if (si->getOperand(0)->getType()->isDoubleTy()) {
						errs() << f->getName() << "." << bb->getName() << ":" <<
							*ins << "\n";
						accesses.push_back(si->getPointerOperand());
					}
				}
			}
		}
	}
	assert(accesses.size() == 8);

	for (size_t i = 0; i < accesses.size(); ++i) {
		for (size_t j = i + 1; j < accesses.size(); ++j) {
			AdvancedAlias &AA = getAnalysis<AdvancedAlias>();
			errs() << "accesses[" << i << "] and accesses[" << j <<
				"] don't alias? ...";
			assert(AA.alias(accesses[i], 0, accesses[j], 0) == AliasAnalysis::NoAlias);
			print_pass(errs());
		}
	}
}

void IntTest::test_test_range(const Module &M) {
	
	if (Program != "test-range")
		return;
	TestBanner X("test-range");

	ExecOnce &EO = getAnalysis<ExecOnce>();
	vector<const Value *> accesses;
	forallconst(Module, f, M) {
		if (EO.not_executed(f))
			continue;
		if (is_main(f))
			continue;
		forallconst(Function, bb, *f) {
			forallconst(BasicBlock, ins, *bb) {
				if (const StoreInst *si = dyn_cast<StoreInst>(ins)) {
					if (si->getOperand(0)->getType()->isDoubleTy()) {
						errs() << f->getName() << "." << bb->getName() << ":" <<
							*ins << "\n";
						accesses.push_back(si->getPointerOperand());
					}
				}
			}
		}
	}
	assert(accesses.size() == 2);

	for (size_t i = 0; i < accesses.size(); ++i) {
		for (size_t j = i + 1; j < accesses.size(); ++j) {
			AdvancedAlias &AA = getAnalysis<AdvancedAlias>();
			errs() << "accesses[" << i << "] and accesses[" << j <<
				"] don't alias? ...";
			assert(AA.alias(accesses[i], 0, accesses[j], 0) == AliasAnalysis::NoAlias);
			print_pass(errs());
		}
	}
}

void IntTest::test_test_malloc(const Module &M) {

	if (Program != "test-malloc")
		return;
	TestBanner X("test-malloc");

	ExecOnce &EO = getAnalysis<ExecOnce>();
	vector<const Value *> accesses;
	forallconst(Module, f, M) {
		if (EO.not_executed(f))
			continue;
		forallconst(Function, bb, *f) {
			forallconst(BasicBlock, ins, *bb) {
				if (const StoreInst *si = dyn_cast<StoreInst>(ins)) {
					if (const ConstantInt *ci = dyn_cast<ConstantInt>(si->getOperand(0))) {
						if (ci->equalsInt(5))
							accesses.push_back(si->getPointerOperand());
					}
				}
			}
		}
	}

	for (size_t i = 0; i < accesses.size(); ++i)
		dbgs() << "Access " << i << ":" << *accesses[i] << "\n";

	AdvancedAlias &AA = getAnalysis<AdvancedAlias>();
	for (size_t i = 0; i < accesses.size(); ++i) {
		for (size_t j = i + 1; j < accesses.size(); ++j) {
			errs() << "Access " << i << " and access " << j << " don't alias? ...";
			assert(AA.alias(accesses[i], 0, accesses[j], 0) == AliasAnalysis::NoAlias);
			print_pass(errs());
		}
	}
}

void IntTest::test_test_array(const Module &M) {
	if (Program != "test-array")
		return;
	TestBanner X("test-array");

	for (Module::const_global_iterator gi = M.global_begin();
			gi != M.global_end(); ++gi) {
		if (gi->getName() == "global_arr") {
			errs() << "Found global_arr\n";
			assert(gi->hasInitializer());
			assert(isa<ConstantAggregateZero>(gi->getInitializer()));
		}
	}
}

void IntTest::test_test_thread(const Module &M) {

	if (Program != "test-thread")
		return;
	TestBanner X("test-thread");

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

void IntTest::test_test_bound(const Module &M) {
	if (Program != "test-bound")
		return;
	TestBanner X("test-bound");

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

void IntTest::test_test_reducer(const Module &M) {
	if (Program != "test-reducer")
		return;
	TestBanner X("test-reducer");

	SolveConstraints &SC = getAnalysis<SolveConstraints>();

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
						errs() << "GEP:" << *gep << "\n";
						const IntegerType *int_type = IntegerType::get(M.getContext(), 32);
						errs() << "argc - 1 >= 0? ...";
						assert(SC.provable(CmpInst::ICMP_SGT,
									&gep->getOperandUse(1),
									dyn_cast<Value>(ConstantInt::get(int_type, 0))));
						print_pass(errs());
					}
				}
			}
		}
	}
}

void IntTest::test_test_loop_2(const Module &M) {
	if (Program != "test-loop-2")
		return;
	TestBanner X("test-loop-2");

	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	
	const Instruction *indvar = NULL;
	forallconst(Module, f, M) {
		forallconst(Function, bb, *f) {
			forallconst(BasicBlock, ins, *bb) {
				if (isa<PHINode>(ins)) {
					assert(!indvar);
					indvar = ins;
				}
			}
		}
	}
	assert(indvar);

	BasicBlock::const_iterator next = indvar; 
	while (next != next->getParent()->end()) {
		if (next->getOpcode() == Instruction::Add)
			break;
		++next;
	}
	assert(next != next->getParent()->end());

	errs() << "Shouldn't be able to prove indvar != next? ...";
	assert(!SC.provable(CmpInst::ICMP_NE,
				dyn_cast<Value>(indvar),
				dyn_cast<Value>((const Instruction *)next)));
	print_pass(errs());
}

void IntTest::test_test_loop(const Module &M) {
	if (Program != "test-loop")
		return;
	TestBanner X("test-loop");

	ExecOnce &EO = getAnalysis<ExecOnce>();

	unsigned n_printfs = 0;
	forallconst(Module, f, M) {
		if (EO.not_executed(f))
			continue;
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

void IntTest::test_radix_like(const Module &M) {
	if (Program != "RADIX-like")
		return;
	TestBanner X("RADIX-like");

	test_radix_common(M);

	DenseMap<const Function *, vector<const Value *> > accesses_to_me;
	vector<const Value *> accesses_to_ff;
	ExecOnce &EO = getAnalysis<ExecOnce>();
	forallconst(Module, f, M) {

		if (EO.not_executed(f))
			continue;

		// Identify accesses to <rank_me>. 
		forallconst(Function, bb, *f) {
			forallconst(BasicBlock, ins, *bb) {
				CallSite cs = CallSite::get(
						const_cast<Instruction *>((const Instruction *)ins));
				if (cs.getInstruction()) {
					const Function *callee = cs.getCalledFunction();
					if (callee && callee->getName() == "printf") {
						assert(cs.arg_size() > 0);
						const Value *my_key = cs.getArgument(cs.arg_size() - 1);
						const User *sext = NULL;
						for (Value::use_const_iterator ui = my_key->use_begin();
								ui != my_key->use_end(); ++ui) {
							if (*ui == ins)
								continue;
							assert(!sext);
							sext = *ui;
						}
						assert(sext);
						for (Value::use_const_iterator ui = sext->use_begin();
								ui != sext->use_end(); ++ui) {
							if (const GetElementPtrInst *gep =
									dyn_cast<GetElementPtrInst>(*ui)) {
								accesses_to_me[f].push_back(gep);
							}
						}
					}
				}
			}
		}

		// Identify accesses to <rank_ff>. 
		// Look at all "store 0". Not all of them are accesses to <rank_ff>,
		// but doesn't affect the result. 
		forallconst(Function, bb, *f) {
			forallconst(BasicBlock, ins, *bb) {
				if (const StoreInst *si = dyn_cast<StoreInst>(ins)) {
					if (const ConstantInt *ci = dyn_cast<ConstantInt>(si->getOperand(0))) {
						if (ci->isZero()) {
							const GetElementPtrInst *gep =
								dyn_cast<GetElementPtrInst>(si->getPointerOperand());
							accesses_to_ff.push_back(gep->getOperand(0));
						}
					}
				}
			}
		}
	}

	errs() << "=== Accesses to rank_me ===\n";
	for (DenseMap<const Function *, vector<const Value *> >::iterator
			i = accesses_to_me.begin(); i != accesses_to_me.end(); ++i) {
		errs() << "Function " << i->first->getName() << ":\n";
		for (size_t j = 0; j < i->second.size(); ++j)
			errs() << *i->second[j] << "\n";
	}

	errs() << "=== Accesses to rank_ff ===\n";
	for (size_t i = 0; i < accesses_to_ff.size(); ++i)
		errs() << *accesses_to_ff[i] << "\n";

	AdvancedAlias &AA = getAnalysis<AdvancedAlias>();
	// SolveConstraints &SC = getAnalysis<SolveConstraints>();
	for (DenseMap<const Function *, vector<const Value *> >::iterator
			i1 = accesses_to_me.begin(); i1 != accesses_to_me.end(); ++i1) {
		DenseMap<const Function *, vector<const Value *> >::iterator i2;
		for (i2 = i1, ++i2; i2 != accesses_to_me.end(); ++i2) {
			for (size_t j1 = 0; j1 < i1->second.size(); ++j1) {
				for (size_t j2 = 0; j2 < i2->second.size(); ++j2) {
					errs() << "{" << i1->first->getName() << ":" << j1 << "} != {" <<
						i2->first->getName() << ":" << j2 << "}? ...";
					assert(AA.alias(i1->second[j1], 0, i2->second[j2], 0) ==
							AliasAnalysis::NoAlias);
					print_pass(errs());
				}
			}
		}
	}
	for (size_t i1 = 0; i1 < accesses_to_ff.size(); ++i1) {
		for (size_t i2 = i1 + 1; i2 < accesses_to_ff.size(); ++i2) {
			errs() << "accesses_to_ff[" << i1 << "] != accesses_to_ff[" <<
				i2 << "]? ...";
			assert(AA.alias(accesses_to_ff[i1], 0, accesses_to_ff[i2], 0) ==
					AliasAnalysis::NoAlias);
			print_pass(errs());
		}
	}
}

void IntTest::test_radix(const Module &M) {
	
	if (Program != "RADIX")
		return;
	TestBanner X("RADIX");

	test_radix_common(M);

	// rank_me_mynum and rank_ff_mynum
	vector<const Value *> ranks;
	vector<const Value *> arr_accesses;
	forallconst(Module, f, M) {
		forallconst(Function, bb, *f) {
			forallconst(BasicBlock, ins, *bb) {
				if (ins->getOpcode() == Instruction::AShr) {
					BasicBlock::const_iterator next = ins;
					++next;
					if (const GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(next)) {
						arr_accesses.push_back(gep);
						ranks.push_back(gep->getOperand(0));
					}
				}
			}
		}
	}
	for (size_t i = 0; i < ranks.size(); ++i)
		dbgs() << "Rank " << i << ":" << *ranks[i] << "\n";
	for (size_t i = 0; i < arr_accesses.size(); ++i)
		dbgs() << "Array access " << i << ":" << *arr_accesses[i] << "\n";

	AdvancedAlias &AA = getAnalysis<AdvancedAlias>();
	for (size_t i = 0; i < ranks.size(); ++i) {
		for (size_t j = i + 1; j < ranks.size(); ++j) {
			errs() << "Comparing rank " << i << " and rank " << j << " ... ";
			assert(AA.alias(ranks[i], 0, ranks[j], 0) == AliasAnalysis::NoAlias);
			print_pass(errs());

			errs() << "Comparing array access " << i << " and array access " <<
				j << "...";
			assert(AA.alias(arr_accesses[i], 0, arr_accesses[j], 0) ==
					AliasAnalysis::NoAlias);
			print_pass(errs());
		}
	}
}

void IntTest::test_radix_common(const Module &M) {
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

void IntTest::test_test_overwrite_2(const Module &M) {
	if (Program != "test-overwrite-2")
		return;
	TestBanner X("test-overwrite-2");

	ExecOnce &EO = getAnalysis<ExecOnce>();
	SolveConstraints &SC = getAnalysis<SolveConstraints>();

	vector<const LoadInst *> loads;
	forallconst(Module, f, M) {
		if (EO.not_executed(f))
			continue;
		forallconst(Function, bb, *f) {
			forallconst(BasicBlock, ins, *bb) {
				if (const LoadInst *li = dyn_cast<LoadInst>(ins)) {
					const Value *p = li->getPointerOperand();
					if (p->getName() == "n")
						loads.push_back(li);
				}
			}
		}
	}

	for (size_t i = 0; i + 1 < loads.size(); ++i) {
		errs() << "loads[" << i << "] == loads[" << (i + 1) << "]? ...";
		assert(SC.provable(CmpInst::ICMP_EQ,
					dyn_cast<Value>(loads[i]),
					dyn_cast<Value>(loads[i + 1])));
		print_pass(errs());
	}
}

void IntTest::test_test_overwrite(const Module &M) {
	if (Program != "test-overwrite")
		return;
	TestBanner X("test-overwrite");

	ExecOnce &EO = getAnalysis<ExecOnce>();

	const Value *v1 = NULL, *v2 = NULL;
	forallconst(Module, f, M) {
		if (EO.not_executed(f))
			continue;
		forallconst(Function, bb, *f) {
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

void IntTest::test_aget_like(const Module &M) {

	if (Program != "aget-like")
		return;
	TestBanner X("aget-like");

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

void IntTest::test_aget(const Module &M) {

	if (Program != "aget")
		return;
	TestBanner X("aget");

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
				SC.set_print_counterexample(true);
				assert(SC.provable(CmpInst::ICMP_SLE, soffsets[i], ranges[i][j].first));
				SC.set_print_counterexample(false);
				print_pass(errs());
			}
			if (foffsets[i]) {
				errs() << "  offset < foffset? ...";
				assert(SC.provable(CmpInst::ICMP_SLT, ranges[i][j].first, foffsets[i]));
				print_pass(errs());
				errs() << "  offset + len <= foffset? ...";
				assert(SC.provable(new Clause(new BoolExpr(CmpInst::ICMP_SLE,
									new Expr(Instruction::Add,
										new Expr(ranges[i][j].first),
										new Expr(ranges[i][j].second)),
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

void IntTest::test_fft(const Module &M) {
	if (Program != "FFT")
		return;
	TestBanner X("FFT");

	test_fft_common(M);
}

void IntTest::test_fft_like(const Module &M) {
	if (Program != "FFT-like")
		return;
	TestBanner X("FFT-like");

	test_fft_common(M);
}

void IntTest::test_fft_common(const Module &M) {
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

void IntTest::test_fft_tern(const Module &M) {
	if (Program != "FFT-tern")
		return;
	TestBanner X("FFT-tern");

	IDAssigner &IDA = getAnalysis<IDAssigner>();
	AdvancedAlias &AA = getAnalysis<AdvancedAlias>();
	SolveConstraints &SC = getAnalysis<SolveConstraints>();

	const Value *v1 = IDA.getValue(1364);
	const Value *v2 = IDA.getValue(2311);
	
	SC.set_print_asserts(true);
	AA.may_alias(v1, v2);
	SC.set_print_asserts(false);
}
