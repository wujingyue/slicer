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

void IntTest::test_fft(const Module &M) {
	TestBanner X("FFT");
	test_fft_common(M);
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
