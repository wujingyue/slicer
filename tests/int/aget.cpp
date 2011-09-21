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

void IntTest::test_aget(const Module &M) {
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
			assert(SC.provable(CmpInst::ICMP_SLE,
						ConstInstList(), soffsets[i], ConstInstList(), foffsets[i]));
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
}

void IntTest::test_aget_like(const Module &M) {
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
