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

void IntTest::fft(Module &M) {
	TestBanner X("FFT");
	fft_common(M);
	
	check_transpose(M);
	check_fft1donce(M);
}

void IntTest::check_fft1donce(Module &M) {
	Function *fft1donce = M.getFunction("FFT1DOnce");
	assert(fft1donce);

	DenseMap<Function *, InstList> thread_contexts;
	for (Value::use_iterator ui = fft1donce->use_begin();
			ui != fft1donce->use_end(); ++ui) {
		if (CallInst *ci = dyn_cast<CallInst>(*ui)) {
			// Make sure FFT1DOnce is the called function instead of an argument.
			if (ci->getCalledFunction() == fft1donce) {
				Function *caller = ci->getParent()->getParent();
				if (caller->getName().startswith("SlaveStart.SLICER"))
					thread_contexts[caller].push_back(ci);
			}
		}
	}
	assert(thread_contexts.size() >= 2 && "Not enough calling contexts to test");

	Instruction *context0 = thread_contexts.begin()->second[0];
	Instruction *context1 = (++thread_contexts.begin())->second[0];
	errs() << "context0 is in function " <<
		context0->getParent()->getParent()->getName() << "\n";
	errs() << "context1 is in function " <<
		context1->getParent()->getParent()->getName() << "\n";
	AdvancedAlias &AAA = getAnalysis<AdvancedAlias>();
	for (Function::iterator bb = fft1donce->begin();
			bb != fft1donce->end(); ++bb) {
		for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
			if (StoreInst *si = dyn_cast<StoreInst>(ins)) {
				errs() << "FFT1DOnce:" << *si << "? ...";
				AliasAnalysis::AliasResult res = AAA.alias(
						ConstInstList(1, context0), si->getPointerOperand(),
						ConstInstList(1, context1), si->getPointerOperand());
				if (res == AliasAnalysis::NoAlias)
					print_pass(errs());
				else
					print_fail(errs());
			}
		}
	}
}

void IntTest::check_transpose(Module &M) {
	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	// The ranges in Transpose under different contexts are disjoint. 
	Function *transpose = M.getFunction("Transpose");
	assert(transpose);
	Value *my_first = NULL, *my_last = NULL;
	for (Function::arg_iterator ai = transpose->arg_begin();
			ai != transpose->arg_end(); ++ai) {
		if (ai->getName() == "MyFirst")
			my_first = ai;
		if (ai->getName() == "MyLast")
			my_last = ai;
	}
	assert(my_first && my_last);

	// Collect all contexts of function Transpose. 
	DenseMap<Function *, vector<ConstInstList> > contexts;
	forall(Module, f, M) {
		forall(Function, bb, *f) {
			forall(BasicBlock, ins, *bb) {
				CallSite cs(ins);
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
	DenseMap<Function *, vector<ConstInstList> >::iterator i1, i2;
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

	StoreInst *racy_store = NULL;
	for (Function::iterator bb = transpose->begin();
			bb != transpose->end(); ++bb) {
		for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
			if (StoreInst *si = dyn_cast<StoreInst>(ins)) {
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
			if (i1->first->getName() != "main" && i2->first->getName() != "main")
				continue;
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

void IntTest::fft_like(Module &M) {
	TestBanner X("FFT-like");
	fft_common(M);
}

void IntTest::fft_common(Module &M) {
	// MyNum's are distinct. 
	vector<Value *> local_ids;
	forall(Module, f, M) {
		if (getAnalysis<ExecOnce>().not_executed(f))
			continue;
		if (!starts_with(f->getName(), "SlaveStart.SLICER"))
			continue;

		Instruction *start = NULL;
		for (BasicBlock::iterator ins = f->getEntryBlock().begin();
				ins != f->getEntryBlock().end(); ++ins) {
			if (CallInst *ci = dyn_cast<CallInst>(ins)) {
				Function *callee = ci->getCalledFunction();
				if (callee && callee->getName() == "pthread_mutex_lock") {
					start = ci;
					break;
				}
			}
		}
		assert(start && "Cannot find a pthread_mutex_lock in the entry block");

		Value *local_id = NULL;
		for (BasicBlock::iterator ins = start;
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
	DenseMap<Function *, vector<ValuePair> > function_ranges;
	forall(Module, f, M) {
		forall(Function, bb, *f) {
			forall(BasicBlock, ins, *bb) {
				CallSite cs(ins);
				if (!cs.getInstruction())
					continue;
				Function *callee = cs.getCalledFunction();
				if (callee && callee->getName() == "Transpose") {
					assert(cs.arg_size() == 7);
					dbgs() << f->getName() << ":" << *ins << "\n";
					function_ranges[f].push_back(make_pair(
								cs.getArgument(4), cs.getArgument(5)));
				}
			}
		}
	}

	DenseMap<Function *, vector<ValuePair> >::iterator i1, i2;
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
