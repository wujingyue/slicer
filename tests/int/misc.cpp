#include "llvm/Module.h"
#include "llvm/Support/Debug.h"
#include "common/util.h"
#include "common/exec-once.h"
using namespace llvm;

#include "int/adv-alias.h"
#include "int/solve.h"
#include "int-test.h"
#include "tests/include/test-utils.h"
using namespace slicer;

void IntTest::test_test_ctxt_4(const Module &M) {
	TestBanner X("test-ctxt-4");

	ExecOnce &EO = getAnalysis<ExecOnce>();
	AdvancedAlias &AA = getAnalysis<AdvancedAlias>();

	const Function *access = M.getFunction("access");
	ConstInstList call_sites;
	for (Module::const_iterator f = M.begin(); f != M.end(); ++f) {
		if (EO.not_executed(f))
			continue;
		for (Function::const_iterator bb = f->begin(); bb != f->end(); ++bb) {
			for (BasicBlock::const_iterator ins = bb->begin();
					ins != bb->end(); ++ins) {
				if (const CallInst *ci = dyn_cast<CallInst>(ins)) {
					if (ci->getCalledFunction() == access)
						call_sites.push_back(ci);
				}
			}
		}
	}

	const Value *loc = NULL;
	for (Function::const_iterator bb = access->begin();
			bb != access->end(); ++bb) {
		for (BasicBlock::const_iterator ins = bb->begin(); ins != bb->end(); ++ins) {
			if (const StoreInst *si = dyn_cast<StoreInst>(ins)) {
				assert(!loc);
				loc = si->getPointerOperand();
			}
		}
	}
	assert(loc);

	for (size_t i = 0; i < call_sites.size(); ++i) {
		for (size_t j = i + 1; j < call_sites.size(); ++j) {
			ConstInstList ci(1, call_sites[i]), cj(1, call_sites[j]);
			errs() << "Access " << i << " and access " << j << " don't alias? ...";
			assert(AA.alias(ci, loc, cj, loc) == AliasAnalysis::NoAlias);
			print_pass(errs());
		}
	}
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
	TestBanner X("test-dep");
	test_test_dep_common(M);
}

void IntTest::test_test_range_4(const Module &M) {
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
				ConstInstList(), dyn_cast<Value>(phi),
				ConstInstList(), dyn_cast<Value>(ConstantInt::get(int_type, 0))));
	assert(SC.provable(CmpInst::ICMP_SLE,
				ConstInstList(), dyn_cast<Value>(phi),
				ConstInstList(), dyn_cast<Value>(ConstantInt::get(int_type, 110))));
}

void IntTest::test_test_dep_common(const Module &M) {
	ExecOnce &EO = getAnalysis<ExecOnce>();
	AdvancedAlias &AA = getAnalysis<AdvancedAlias>();

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
			errs() << "{" << i1->first->getName() << ":" << j1<< "} and {" <<
				i2->first->getName() << ":" << j2 << "} don't alias? ...";
			assert(AA.alias(i1->second[j1], 0, i2->second[j2], 0) ==
					AliasAnalysis::NoAlias);
			print_pass(errs());
		}
	}
}

void IntTest::test_test_range_3(const Module &M) {
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

void IntTest::test_test_thread_2(const Module &M) {
	TestBanner X("test-thread-2");
	print_pass(errs());
}

void IntTest::test_test_thread(const Module &M) {
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
			assert(SC.provable(CmpInst::ICMP_NE,
						ConstInstList(), local_ids[i], ConstInstList(), local_ids[j]));
			print_pass(errs());
		}
	}
}

void IntTest::test_test_bound(const Module &M) {
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
					ConstInstList(), &gep1->getOperandUse(2),
					ConstInstList(), &gep2->getOperandUse(2)));
		print_pass(errs());
		errs() << "gep1 and gep2 alias? ...";
		assert(AAA.alias(gep1, 0, gep2, 0) == AliasAnalysis::NoAlias);
		print_pass(errs());
	}
}

void IntTest::test_test_reducer(const Module &M) {
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
									ConstInstList(), &gep->getOperandUse(1),
									ConstInstList(),
									dyn_cast<Value>(ConstantInt::get(int_type, 0))));
						print_pass(errs());
					}
				}
			}
		}
	}
}

void IntTest::test_test_loop_2(const Module &M) {
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
				ConstInstList(), dyn_cast<Value>(indvar),
				ConstInstList(), dyn_cast<Value>((const Instruction *)next)));
	print_pass(errs());
}

void IntTest::test_test_loop(const Module &M) {
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

void IntTest::test_test_overwrite_2(const Module &M) {
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
					ConstInstList(), dyn_cast<Value>(loads[i]),
					ConstInstList(), dyn_cast<Value>(loads[i + 1])));
		print_pass(errs());
	}
}

void IntTest::test_test_overwrite(const Module &M) {
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
	assert(SC.provable(CmpInst::ICMP_EQ,
				ConstInstList(), v1, ConstInstList(), v2));
	print_pass(errs());
}
