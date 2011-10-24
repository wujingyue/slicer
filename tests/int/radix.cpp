#include "llvm/Module.h"
#include "llvm/Support/Debug.h"
#include "common/util.h"
#include "common/exec-once.h"
using namespace llvm;

#include "slicer/adv-alias.h"
#include "slicer/test-utils.h"
#include "slicer/solve.h"
#include "int-test.h"
using namespace slicer;

void IntTest::test_radix_like(const Module &M) {
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
	TestBanner X("RADIX");

	test_radix_common(M);

#if 0
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
#endif
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
			assert(SC.provable(CmpInst::ICMP_NE,
						ConstInstList(), local_ids[i],
						ConstInstList(), local_ids[j]));
			print_pass(errs());
		}
	}
}
