#include "llvm/Module.h"
#include "llvm/Support/Debug.h"
#include "common/util.h"
#include "common/exec-once.h"
using namespace llvm;

#include "slicer/adv-alias.h"
#include "slicer/solve.h"
#include "slicer/test-utils.h"
#include "int-test.h"
using namespace slicer;

void IntTest::aget(Module &M) {
	TestBanner X("aget");

	vector<Function *> thr_funcs;
	forall(Module, f, M) {
		if (starts_with(f->getName(), "http_get.SLICER"))
			thr_funcs.push_back(f);
	}
	dbgs() << "Thread functions:";
	for (size_t i = 0; i < thr_funcs.size(); ++i)
		dbgs() << " " << thr_funcs[i]->getName();
	dbgs() << "\n";
	
	vector<Value *> soffsets(thr_funcs.size(), NULL);
	vector<Value *> foffsets(thr_funcs.size(), NULL);
	for (size_t i = 0; i < thr_funcs.size(); ++i) {
		Function *f = thr_funcs[i];
		assert(distance(f->arg_begin(), f->arg_end()) == 1);
		Value *td = f->arg_begin();
		for (Value::use_iterator ui = td->use_begin();
				ui != td->use_end(); ++ui) {
			if (GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(*ui)) {
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
							for (Value::use_iterator ui2 = gep->use_begin();
									ui2 != gep->use_end(); ++ui2) {
								if (isa<LoadInst>(*ui2)) {
									soffsets[i] = *ui2;
									break;
								}
							}
						}
						if (index == 3 && foffsets[i] == NULL) {
							for (Value::use_iterator ui2 = gep->use_begin();
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

	vector<vector<UsePair> > ranges;
	ranges.resize(thr_funcs.size());
	for (size_t i = 0; i < thr_funcs.size(); ++i) {
		Function *f = thr_funcs[i];
		forall(Function, bb, *f) {
			forall(BasicBlock, ins, *bb) {
				if (CallInst *ci = dyn_cast<CallInst>(ins)) {
					Function *callee = ci->getCalledFunction();
					if (callee && callee->getName() == "pwrite") {
						// pwrite(???, ???, len, offset)
						assert(ci->getNumOperands() == 5);
						ranges[i].push_back(make_pair(
									&ci->getOperandUse(3), &ci->getOperandUse(2)));
					}
					if (callee && callee->getName() == "fake_pwrite") {
						// fake_pwrite(???, offset, len)
						assert(ci->getNumOperands() == 4);
						ranges[i].push_back(make_pair(
									&ci->getOperandUse(1), &ci->getOperandUse(2)));
					}
				}
			}
		}
		dbgs() << "Ranges in thread " << i << ":\n";
		for (size_t j = 0; j < ranges[i].size(); ++j) {
			User *user = ranges[i][j].first->getUser();
			assert(user == ranges[i][j].second->getUser());
			dbgs() << *user << "\n";
		}
	}
	

	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	for (size_t i = 0; i < thr_funcs.size(); ++i) {
		if (soffsets[i] && foffsets[i]) {
			errs() << "soffsets[" << i << "] <= foffsets[" << i << "]? ...";
			assert(SC.provable(CmpInst::ICMP_SLE,
						ConstInstList(), soffsets[i],
						ConstInstList(), foffsets[i]));
			print_pass(errs());
		}
	}
	
	for (size_t i = 0; i < thr_funcs.size(); ++i) {
		for (size_t j = 0; j < ranges[i].size(); ++j) {
			errs() << "Range {" << i << ", " << j << "}\n";
			if (soffsets[i]) {
				errs() << "  soffsets <= offset? ...";
				SC.set_print_counterexample(true);
				assert(SC.provable(CmpInst::ICMP_SLE,
							ConstInstList(), soffsets[i],
							ConstInstList(), ranges[i][j].first));
				SC.set_print_counterexample(false);
				print_pass(errs());
			}
			if (foffsets[i]) {
				errs() << "  offset < foffset? ...";
				assert(SC.provable(CmpInst::ICMP_SLT,
							ConstInstList(), ranges[i][j].first,
							ConstInstList(), foffsets[i]));
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

	// fake_pwrite
	// check_fake_pwrite(M);
	check_fake_pwrite_cs(M);
}

void IntTest::check_fake_pwrite_cs(Module &M) {
	AdvancedAlias &AA = getAnalysis<AdvancedAlias>();

	DenseMap<Function *, InstList> thread_call_sites;
	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		if (starts_with(f->getName(), "http_get.SLICER")) {
			for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
				for (BasicBlock::iterator ins = bb->begin();
						ins != bb->end(); ++ins) {
					if (CallInst *ci = dyn_cast<CallInst>(ins)) {
						Function *callee = ci->getCalledFunction();
						if (callee && callee->getName() == "fake_pwrite")
							thread_call_sites[f].push_back(ci);
					}
				}
			}
		}
	}

	StoreInst *the_store = NULL;
	Function *fake_pwrite = M.getFunction("fake_pwrite");
	assert(fake_pwrite);
	for (Function::iterator bb = fake_pwrite->begin(); bb != fake_pwrite->end();
			++bb) {
		for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
			if (StoreInst *si = dyn_cast<StoreInst>(ins)) {
				assert(the_store == NULL);
				the_store = si;
			}
		}
	}
	assert(the_store);

	for (DenseMap<Function *, InstList>::iterator
			i1 = thread_call_sites.begin(); i1 != thread_call_sites.end(); ++i1) {
		DenseMap<Function *, InstList>::iterator i2 = i1;
		for (++i2; i2 != thread_call_sites.end(); ++i2) {
			for (size_t j1 = 0; j1 < i1->second.size(); ++j1) {
				for (size_t j2 = 0; j2 < i2->second.size(); ++j2) {
					errs() << "CS " << j1 << ", " << j2 << ": ";
					assert(AA.alias(
								ConstInstList(1, i1->second[j1]),
								the_store->getPointerOperand(),
								ConstInstList(1, i2->second[j2]),
								the_store->getPointerOperand()) == AliasAnalysis::NoAlias);
					print_pass(errs());
				}
			}
		}
	}
}

void IntTest::check_fake_pwrite(Module &M) {
	AliasAnalysis &AA = getAnalysis<AdvancedAlias>();

	Value *fake_buffer = M.getNamedGlobal("fake_buffer");
	assert(fake_buffer);
	
	InstList loads;
	for (Value::use_iterator ui = fake_buffer->use_begin();
			ui != fake_buffer->use_end(); ++ui) {
		if (LoadInst *li = dyn_cast<LoadInst>(*ui))
			loads.push_back(li);
	}

	InstList geps;
	for (size_t i = 0; i < loads.size(); ++i) {
		Instruction *li = loads[i];
		for (Value::use_iterator ui = li->use_begin(); ui != li->use_end(); ++ui) {
			if (GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(*ui))
				geps.push_back(gep);
		}
	}

	InstList stores;
	for (size_t i = 0; i < geps.size(); ++i) {
		Instruction *gep = geps[i];
		for (Value::use_iterator ui = gep->use_begin(); ui != gep->use_end(); ++ui) {
			if (StoreInst *si = dyn_cast<StoreInst>(*ui))
				stores.push_back(si);
		}
	}

	DenseMap<Function *, InstList> thread_stores;
	for (size_t i = 0; i < stores.size(); ++i) {
		Function *f = stores[i]->getParent()->getParent();
		if (starts_with(f->getName(), "http_get.SLICER")) {
			thread_stores[f].push_back(stores[i]);
			dbgs() << "Store: " << f->getName() << ":" << *stores[i] << "\n";
		}
	}

	for (DenseMap<Function *, InstList>::iterator i1 = thread_stores.begin();
			i1 != thread_stores.end(); ++i1) {
		DenseMap<Function *, InstList>::iterator i2 = i1;
		for (++i2; i2 != thread_stores.end(); ++i2) {
			for (size_t j1 = 0; j1 < i1->second.size(); ++j1) {
				for (size_t j2 = 0; j2 < i2->second.size(); ++j2) {
					StoreInst *s1 = dyn_cast<StoreInst>(i1->second[j1]);
					StoreInst *s2 = dyn_cast<StoreInst>(i2->second[j2]);
					assert(s1 && s2);
					AliasAnalysis::AliasResult res = AA.alias(
							s1->getPointerOperand(), s2->getPointerOperand());
					assert(res == AliasAnalysis::NoAlias);
					print_pass(errs());
				}
			}
		}
	}
}

void IntTest::aget_like(Module &M) {
	TestBanner X("aget-like");

	vector<vector<UsePair> > ranges;
	forall(Module, f, M) {
		if (!starts_with(f->getName(), "http_get.SLICER"))
			continue;
		errs() << "=== Function " << f->getName() << " ===\n";
		
		ranges.push_back(vector<UsePair>());
		forall(Function, bb, *f) {
			forall(BasicBlock, ins, *bb) {
				if (CallInst *ci = dyn_cast<CallInst>(ins)) {
					Function *callee = ci->getCalledFunction();
					if (callee && callee->getName() == "fake_write") {
						// fake_write(buffer, size, offset)
						errs() << *ci << "\n";
						ranges.back().push_back(make_pair(
									&ci->getOperandUse(2),
									&ci->getOperandUse(1)));
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
