/**
 * Author: Jingyue
 */

#include "llvm/Module.h"
#include "llvm/Support/Debug.h"
using namespace llvm;

#include "common/util.h"
#include "common/exec-once.h"
using namespace rcs;

#include "slicer/adv-alias.h"
#include "slicer/solve.h"
#include "slicer/test-utils.h"
#include "int-test.h"
using namespace slicer;

void IntTest::test_alloca(Module &M) {
	TestBanner X("test-alloca");

	Function *worker1 = M.getFunction("worker1.SLICER"); assert(worker1);
	Function *worker2 = M.getFunction("worker2.SLICER"); assert(worker2);

	Value *p1 = NULL, *p2 = NULL;
	for (Function::iterator bb = worker1->begin(); bb != worker1->end(); ++bb) {
		for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
			if (StoreInst *si = dyn_cast<StoreInst>(ins)) {
				assert(!p1);
				p1 = si->getPointerOperand();
			}
		}
	}
	for (Function::iterator bb = worker2->begin(); bb != worker2->end(); ++bb) {
		for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
			if (StoreInst *si = dyn_cast<StoreInst>(ins)) {
				assert(!p2);
				p2 = si->getPointerOperand();
			}
		}
	}
	assert(p1 && p2);

	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	errs() << "p1 != p2? ...";
	assert(SC.provable(CmpInst::ICMP_NE,
			ConstInstList(), p1,
			ConstInstList(), p2));
	print_pass(errs());
}

void IntTest::test_path_2(Module &M) {
	TestBanner X("test-path-2");

	Function *main = M.getFunction("main"); assert(main);
	CallInst *the_printf = NULL;
	for (Function::iterator bb = main->begin(); bb != main->end(); ++bb) {
		for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
			if (CallInst *ci = dyn_cast<CallInst>(ins)) {
				Function *callee = ci->getCalledFunction();
				if (callee && callee->getName() == "printf") {
					assert(the_printf == NULL);
					the_printf = ci;
				}
			}
		}
	}
	assert(the_printf);

	unsigned n_args = the_printf->getNumArgOperands();
	assert(n_args > 0);
	Value *arg = the_printf->getArgOperand(n_args - 1);

	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	errs() << "printed value = 1? ...";
	assert(SC.provable(CmpInst::ICMP_EQ,
				ConstInstList(), arg,
				ConstInstList(), dyn_cast<Value>(ConstantInt::get(int_type, 1))));
	print_pass(errs());
}

void IntTest::test_barrier(Module &M) {
	TestBanner X("test-barrier");

	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	
	Function *print = M.getFunction("printf"); assert(print);
	for (Value::use_iterator ui = print->use_begin(); ui != print->use_end();
			++ui) {
		if (CallInst *ci = dyn_cast<CallInst>(*ui)) {
			unsigned n_args = ci->getNumArgOperands();
			assert(n_args > 0);
			errs() << "printed value = 2? ...";
			assert(SC.provable(CmpInst::ICMP_EQ,
						ConstInstList(), ci->getArgOperand(n_args - 1),
						ConstInstList(), dyn_cast<Value>(ConstantInt::get(int_type, 2))));
			print_pass(errs());
		}
	}
}

void IntTest::test_lcssa(Module &M) {
	TestBanner X("test-lcssa");

	SolveConstraints &SC = getAnalysis<SolveConstraints>();

	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
			for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
				if (CallInst *ci = dyn_cast<CallInst>(ins)) {
					Function *callee = ci->getCalledFunction();
					if (callee && callee->getName() == "printf") {
						unsigned n_args = ci->getNumArgOperands();
						assert(n_args > 0);
						Value *arg = ci->getArgOperand(n_args - 1);
						if (isa<PHINode>(arg)) {
							errs() << "lcssa >= 0? ...";
							assert(SC.provable(CmpInst::ICMP_SGE,
										ConstInstList(),
										arg,
										ConstInstList(),
										dyn_cast<Value>(ConstantInt::get(int_type, 0))));
							print_pass(errs());
						}
					}
				}
			}
		}
	}
}

void IntTest::test_ctxt_4(Module &M) {
	TestBanner X("test-ctxt-4");

	ExecOnce &EO = getAnalysis<ExecOnce>();
	AdvancedAlias &AA = getAnalysis<AdvancedAlias>();

	Function *access = M.getFunction("access");
	ConstInstList call_sites;
	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		if (EO.not_executed(f))
			continue;
		for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
			for (BasicBlock::iterator ins = bb->begin();
					ins != bb->end(); ++ins) {
				if (CallInst *ci = dyn_cast<CallInst>(ins)) {
					if (ci->getCalledFunction() == access)
						call_sites.push_back(ci);
				}
			}
		}
	}

	Value *loc = NULL;
	for (Function::iterator bb = access->begin();
			bb != access->end(); ++bb) {
		for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
			if (StoreInst *si = dyn_cast<StoreInst>(ins)) {
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

void IntTest::test_ctxt_2(Module &M) {
	TestBanner X("test-ctxt-2");

	SolveConstraints &SC = getAnalysis<SolveConstraints>();

	Function *foo = M.getFunction("foo");
	assert(foo && "Cannot find function <foo>");
	Value *b = NULL;
	forall(Function, bb, *foo) {
		forall(BasicBlock, ins, *bb) {
			if (ins->getOpcode() == Instruction::Add) {
				assert(!b && "Multiple variable <b>");
				b = ins;
			}
		}
	}
	assert(b && "Cannot find variable <b>");

	errs() << "b == 2 or 3? ...";
	assert(SC.provable(new Clause(Instruction::Or,
					new Clause(new BoolExpr(CmpInst::ICMP_EQ,
							new Expr(b, 1), new Expr(ConstantInt::get(int_type, 2)))),
					new Clause(new BoolExpr(CmpInst::ICMP_EQ,
							new Expr(b, 1), new Expr(ConstantInt::get(int_type, 3)))))));
	print_pass(errs());
}

void IntTest::test_dep(Module &M) {
	TestBanner X("test-dep");
	test_dep_common(M);
}

void IntTest::test_range_4(Module &M) {
	TestBanner X("test-range-4");

	PHINode *phi = NULL;
	forall(Module, f, M) {
		forall(Function, bb, *f) {
			forall(BasicBlock, ins, *bb) {
				if (PHINode *p = dyn_cast<PHINode>(ins)) {
					assert(!phi);
					phi = p;
				}
			}
		}
	}
	assert(phi);

	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	assert(SC.provable(CmpInst::ICMP_SGE,
				ConstInstList(), dyn_cast<Value>(phi),
				ConstInstList(), dyn_cast<Value>(ConstantInt::get(int_type, 0))));
	assert(SC.provable(CmpInst::ICMP_SLE,
				ConstInstList(), dyn_cast<Value>(phi),
				ConstInstList(), dyn_cast<Value>(ConstantInt::get(int_type, 110))));
}

void IntTest::test_dep_common(Module &M) {
	ExecOnce &EO = getAnalysis<ExecOnce>();
	AliasAnalysis &AA = getAnalysis<AdvancedAlias>();

	DenseMap<Function *, ValueList> accesses;
	forall(Module, f, M) {
		if (EO.not_executed(f))
			continue;
		if (!starts_with(f->getName(), "slave_sort.SLICER"))
			continue;
		forall(Function, bb, *f) {
			forall(BasicBlock, ins, *bb) {
				if (StoreInst *si = dyn_cast<StoreInst>(ins)) {
					if (ConstantInt *ci =
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

	DenseMap<Function *, ValueList>::iterator i1, i2;
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

void IntTest::test_range_3(Module &M) {
	TestBanner X("test-range-3");

	ExecOnce &EO = getAnalysis<ExecOnce>();
	DenseMap<Function *, ValueList> accesses;
	forall(Module, f, M) {
		if (EO.not_executed(f))
			continue;
		if (is_main(f))
			continue;
		forall(Function, bb, *f) {
			forall(BasicBlock, ins, *bb) {
				if (StoreInst *si = dyn_cast<StoreInst>(ins)) {
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

	DenseMap<Function *, ValueList>::iterator i1, i2;
	i1 = accesses.begin();
	i2 = i1; ++i2;

	for (size_t j1 = 0; j1 < i1->second.size(); ++j1) {
		for (size_t j2 = 0; j2 < i2->second.size(); ++j2) {
			AliasAnalysis &AA = getAnalysis<AdvancedAlias>();
			errs() << "{" << i1->first->getName() << ":" << j1<< "} and {" <<
				i2->first->getName() << ":" << j2 << "} don't alias? ...";
			assert(AA.alias(i1->second[j1], 0, i2->second[j2], 0) ==
					AliasAnalysis::NoAlias);
			print_pass(errs());
		}
	}
}

void IntTest::test_range_2(Module &M) {
	TestBanner X("test-range-2");

	ExecOnce &EO = getAnalysis<ExecOnce>();
	vector<Value *> accesses;
	forall(Module, f, M) {
		if (EO.not_executed(f))
			continue;
		if (is_main(f))
			continue;
		forall(Function, bb, *f) {
			forall(BasicBlock, ins, *bb) {
				if (StoreInst *si = dyn_cast<StoreInst>(ins)) {
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
			AliasAnalysis &AA = getAnalysis<AdvancedAlias>();
			errs() << "accesses[" << i << "] and accesses[" << j <<
				"] don't alias? ...";
			assert(AA.alias(accesses[i], 0, accesses[j], 0) == AliasAnalysis::NoAlias);
			print_pass(errs());
		}
	}
}

void IntTest::test_range(Module &M) {
	TestBanner X("test-range");

	ExecOnce &EO = getAnalysis<ExecOnce>();
	vector<Value *> accesses;
	forall(Module, f, M) {
		if (EO.not_executed(f))
			continue;
		if (is_main(f))
			continue;
		forall(Function, bb, *f) {
			forall(BasicBlock, ins, *bb) {
				if (StoreInst *si = dyn_cast<StoreInst>(ins)) {
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
			AliasAnalysis &AA = getAnalysis<AdvancedAlias>();
			errs() << "accesses[" << i << "] and accesses[" << j <<
				"] don't alias? ...";
			assert(AA.alias(accesses[i], 0, accesses[j], 0) == AliasAnalysis::NoAlias);
			print_pass(errs());
		}
	}
}

void IntTest::test_malloc(Module &M) {
	TestBanner X("test-malloc");

	ExecOnce &EO = getAnalysis<ExecOnce>();
	vector<Value *> accesses;
	forall(Module, f, M) {
		if (EO.not_executed(f))
			continue;
		forall(Function, bb, *f) {
			forall(BasicBlock, ins, *bb) {
				if (StoreInst *si = dyn_cast<StoreInst>(ins)) {
					if (ConstantInt *ci = dyn_cast<ConstantInt>(si->getOperand(0))) {
						if (ci->equalsInt(5))
							accesses.push_back(si->getPointerOperand());
					}
				}
			}
		}
	}

	for (size_t i = 0; i < accesses.size(); ++i)
		dbgs() << "Access " << i << ":" << *accesses[i] << "\n";

	AliasAnalysis &AA = getAnalysis<AdvancedAlias>();
	for (size_t i = 0; i < accesses.size(); ++i) {
		for (size_t j = i + 1; j < accesses.size(); ++j) {
			errs() << "Access " << i << " and access " << j << " don't alias? ...";
			assert(AA.alias(accesses[i], 0, accesses[j], 0) == AliasAnalysis::NoAlias);
			print_pass(errs());
		}
	}
}

void IntTest::test_array(Module &M) {
	TestBanner X("test-array");

	bool found = false;
	for (Module::global_iterator gi = M.global_begin();
			gi != M.global_end(); ++gi) {
		if (gi->getName() == "global_arr") {
			found = true;
			assert(gi->hasInitializer());
			assert(isa<ConstantAggregateZero>(gi->getInitializer()));
		}
	}

	errs() << "Found global_arr? ...";
	assert(found);
	print_pass(errs());
}

void IntTest::test_thread_2(Module &M) {
	TestBanner X("test-thread-2");
	print_pass(errs());
}

void IntTest::test_thread(Module &M) {
	TestBanner X("test-thread");

	vector<Value *> local_ids;
	forall(Module, f, M) {
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

void IntTest::test_bound(Module &M) {
	TestBanner X("test-bound");

	forall(Module, f, M) {
		if (f->getName() != "main")
			continue;
		GetElementPtrInst *gep1 = NULL, *gep2 = NULL;
		forall(Function, bb, *f) {
			forall(BasicBlock, ins, *bb) {
				if (GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(ins)) {
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
		AliasAnalysis &AAA = getAnalysis<AdvancedAlias>();

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

void IntTest::test_reducer(Module &M) {
	TestBanner X("test-reducer");

	SolveConstraints &SC = getAnalysis<SolveConstraints>();

	forall(Module, f, M) {
		if (f->getName() != "main")
			continue;
		forall(Function, bb, *f) {
			forall(BasicBlock, ins, *bb) {
				if (CallInst *ci = dyn_cast<CallInst>(ins)) {
					Function *callee = ci->getCalledFunction();
					if (callee && callee->getName() == "printf") {
						BasicBlock::iterator gep = ins;
						for (gep = bb->begin(); gep != ins; ++gep) {
							if (isa<GetElementPtrInst>(gep))
								break;
						}
						assert(isa<GetElementPtrInst>(gep) &&
								"Cannot find a GEP before the printf");
						errs() << "GEP:" << *gep << "\n";
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

void IntTest::test_loop_2(Module &M) {
	TestBanner X("test-loop-2");

	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	
	Instruction *indvar = NULL;
	forall(Module, f, M) {
		forall(Function, bb, *f) {
			forall(BasicBlock, ins, *bb) {
				if (isa<PHINode>(ins)) {
					assert(!indvar);
					indvar = ins;
				}
			}
		}
	}
	assert(indvar);

	BasicBlock::iterator next = indvar; 
	while (next != next->getParent()->end()) {
		if (next->getOpcode() == Instruction::Add)
			break;
		++next;
	}
	assert(next != next->getParent()->end());

	errs() << "Shouldn't be able to prove indvar != next? ...";
	assert(!SC.provable(CmpInst::ICMP_NE,
				ConstInstList(), dyn_cast<Value>(indvar),
				ConstInstList(), dyn_cast<Value>((Instruction *)next)));
	print_pass(errs());
}

void IntTest::test_loop(Module &M) {
	TestBanner X("test-loop");

	ExecOnce &EO = getAnalysis<ExecOnce>();

	unsigned n_printfs = 0;
	forall(Module, f, M) {
		if (EO.not_executed(f))
			continue;
		forall(Function, bb, *f) {
			forall(BasicBlock, ins, *bb) {
				if (CallInst *ci = dyn_cast<CallInst>(ins)) {
					Function *callee = ci->getCalledFunction();
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

void IntTest::test_overwrite_2(Module &M) {
	TestBanner X("test-overwrite-2");

	ExecOnce &EO = getAnalysis<ExecOnce>();
	SolveConstraints &SC = getAnalysis<SolveConstraints>();

	vector<LoadInst *> loads;
	forall(Module, f, M) {
		if (EO.not_executed(f))
			continue;
		forall(Function, bb, *f) {
			forall(BasicBlock, ins, *bb) {
				if (LoadInst *li = dyn_cast<LoadInst>(ins)) {
					Value *p = li->getPointerOperand();
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

void IntTest::test_overwrite(Module &M) {
	TestBanner X("test-overwrite");

	ExecOnce &EO = getAnalysis<ExecOnce>();

	Value *v1 = NULL, *v2 = NULL;
	forall(Module, f, M) {
		if (EO.not_executed(f))
			continue;
		forall(Function, bb, *f) {
			forall(BasicBlock, ins, *bb) {
				if (LoadInst *li = dyn_cast<LoadInst>(ins)) {
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

void IntTest::test_global(Module &M) {
	TestBanner X("test-global");

	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	ExecOnce &EO = getAnalysis<ExecOnce>();

	Function *transpose = M.getFunction("transpose");
	assert(transpose);
	Instruction *the_call = NULL;
	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		if (EO.not_executed(f))
			continue;
		for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
			for (BasicBlock::iterator ins = bb->begin();
					ins != bb->end(); ++ins) {
				if (CallInst *ci = dyn_cast<CallInst>(ins)) {
					if (ci->getCalledFunction() == transpose) {
						the_call = ci;
						break;
					}
				}
			}
			if (the_call)
				break;
		}
		if (the_call)
			break;
	}
	assert(the_call);

	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
			for (BasicBlock::iterator ins = bb->begin();
					ins != bb->end(); ++ins) {
				CallSite cs(ins);
				if (cs.getInstruction()) {
					Function *callee = cs.getCalledFunction();
					if (callee && callee->getName() == "printf") {
						assert(cs.arg_size() > 0);
						Value *v = cs.getArgument(cs.arg_size() - 1);
						errs() << "a + delta <= 3? ...";
						assert(SC.provable(CmpInst::ICMP_SLE,
									ConstInstList(1, the_call), v,
									ConstInstList(),
									dyn_cast<Value>(ConstantInt::get(int_type, 3))));
						print_pass(errs());
					}
				}
			}
		}
	}
}
