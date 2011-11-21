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

void IntTest::test_fft(const Module &M) {
	TestBanner X("FFT");
	test_fft_common(M);
	
	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	// The ranges in Transpose under different contexts are disjoint. 
	const Function *transpose = M.getFunction("Transpose");
	assert(transpose);
	const Value *my_first = NULL, *my_last = NULL;
	for (Function::const_arg_iterator ai = transpose->arg_begin();
			ai != transpose->arg_end(); ++ai) {
		if (ai->getName() == "MyFirst")
			my_first = ai;
		if (ai->getName() == "MyLast")
			my_last = ai;
	}
	assert(my_first && my_last);

	// Collect all contexts of function Transpose. 
	DenseMap<const Function *, vector<ConstInstList> > contexts;
	forallconst(Module, f, M) {
		forallconst(Function, bb, *f) {
			forallconst(BasicBlock, ins, *bb) {
				CallSite cs(const_cast<Instruction *>((const Instruction *)ins));
				if (!cs.getInstruction())
					continue;
				if (cs.getCalledFunction() == transpose) {
					assert(cs.arg_size() == 7);
					dbgs() << f->getName() << ":" << *ins << "\n";
					contexts[f].push_back(ConstInstList(1, ins));
				}
			}
		}
	}

	// Ask if the ranges [MyFirst, MyLast) are disjoint under different contexts.
	DenseMap<const Function *, vector<ConstInstList> >::iterator i1, i2;
	for (i1 = contexts.begin(); i1 != contexts.end(); ++i1) {
		i2 = i1;
		for (++i2; i2 != contexts.end(); ++i2) {
			for (size_t j1 = 0; j1 < i1->second.size(); ++j1) {
				for (size_t j2 = 0; j2 < i2->second.size(); ++j2) {
					Expr *s1 = new Expr(my_first);
					Expr *e1 = new Expr(my_last);
					Expr *s2 = new Expr(my_first);
					Expr *e2 = new Expr(my_last);

					s1->callstack = i1->second[j1]; s1->context = 1;
					e1->callstack = i1->second[j1]; e1->context = 1;
					s2->callstack = i2->second[j2]; s2->context = 2;
					e2->callstack = i2->second[j2]; e2->context = 2;

					Clause *c1 = new Clause(new BoolExpr(CmpInst::ICMP_SLE, e1, s2));
					Clause *c2 = new Clause(new BoolExpr(CmpInst::ICMP_SLE, e2, s1));
					Clause *disjoint = new Clause(Instruction::Or, c1, c2);
					errs() << "Context: {" << i1->first->getName() << ":" << j1 <<
						"} and {" << i2->first->getName() << ":" << j2 <<
						"} are disjoint? ...";
					assert(SC.provable(disjoint));
					print_pass(errs());
					delete disjoint;
				}
			}
		}
	}

	const StoreInst *racy_store = NULL;
	for (Function::const_iterator bb = transpose->begin();
			bb != transpose->end(); ++bb) {
		for (BasicBlock::const_iterator ins = bb->begin(); ins != bb->end(); ++ins) {
			if (const StoreInst *si = dyn_cast<StoreInst>(ins)) {
				if (si->getOperand(0)->getType()->isDoubleTy()) {
					racy_store = si;
					break;
				}
			}
		}
		if (racy_store)
			break;
	}
	assert(racy_store);

	for (i1 = contexts.begin(); i1 != contexts.end(); ++i1) {
		i2 = i1;
		for (++i2; i2 != contexts.end(); ++i2) {
			for (size_t j1 = 0; j1 < i1->second.size(); ++j1) {
				for (size_t j2 = 0; j2 < i2->second.size(); ++j2) {
					AdvancedAlias &AA = getAnalysis<AdvancedAlias>();
					errs() << "Store: {" << i1->first->getName() << ":" << j1
						<< "} and {" << i2->first->getName() << ":" << j2
						<< "} are disjoint? ...";
					assert(
							AA.alias(
								i1->second[j1], racy_store->getPointerOperand(),
								i2->second[j2], racy_store->getPointerOperand()) ==
							AliasAnalysis::NoAlias);
					print_pass(errs());
				}
			}
		}
	}

}

void IntTest::test_fft_like(const Module &M) {
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
			assert(SC.provable(CmpInst::ICMP_NE,
						ConstInstList(), local_ids[i], ConstInstList(), local_ids[j]));
			print_pass(errs());
		}
	}
	
	// The ranges passed to Transpose are disjoint. 
	DenseMap<const Function *, vector<ConstValuePair> > function_ranges;
	forallconst(Module, f, M) {
		forallconst(Function, bb, *f) {
			forallconst(BasicBlock, ins, *bb) {
				CallSite cs(const_cast<Instruction *>((const Instruction *)ins));
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
					errs() << "Call site: {" << i1->first->getName() << ":" << j1 <<
						"} and {" << i2->first->getName() << ":" << j2 <<
						"} are disjoint? ...";
					assert(SC.provable(disjoint));
					print_pass(errs());
					delete disjoint;
				}
			}
		}
	}
}
