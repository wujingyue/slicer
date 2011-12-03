#include "llvm/Module.h"
#include "llvm/Support/Debug.h"
using namespace llvm;

#include "common/util.h"
#include "common/exec-once.h"
using namespace rcs;

#include "slicer/adv-alias.h"
#include "slicer/test-utils.h"
#include "slicer/solve.h"
#include "int-test.h"
using namespace slicer;

void IntTest::radix_like(Module &M) {
	TestBanner X("RADIX-like");

	radix_common(M);

	DenseMap<Function *, ValueList> accesses_to_me;
	vector<Value *> accesses_to_ff;
	ExecOnce &EO = getAnalysis<ExecOnce>();
	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		if (EO.not_executed(f))
			continue;

		// Identify accesses to <rank_me>. 
		for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
			for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
				CallSite cs(ins);
				if (cs.getInstruction()) {
					Function *callee = cs.getCalledFunction();
					if (callee && callee->getName() == "printf") {
						assert(cs.arg_size() > 0);
						Value *my_key = cs.getArgument(cs.arg_size() - 1);
						User *sext = NULL;
						for (Value::use_iterator ui = my_key->use_begin();
								ui != my_key->use_end(); ++ui) {
							if (*ui == ins)
								continue;
							assert(!sext);
							sext = *ui;
						}
						assert(sext);
						for (Value::use_iterator ui = sext->use_begin();
								ui != sext->use_end(); ++ui) {
							if (GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(*ui)) {
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
		for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
			for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
				if (StoreInst *si = dyn_cast<StoreInst>(ins)) {
					if (ConstantInt *ci = dyn_cast<ConstantInt>(si->getOperand(0))) {
						if (ci->isZero()) {
							GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(
									si->getPointerOperand());
							accesses_to_ff.push_back(gep->getOperand(0));
						}
					}
				}
			}
		}
	}

	errs() << "=== Accesses to rank_me ===\n";
	for (DenseMap<Function *, ValueList>::iterator
			i = accesses_to_me.begin(); i != accesses_to_me.end(); ++i) {
		errs() << "Function " << i->first->getName() << ":\n";
		for (size_t j = 0; j < i->second.size(); ++j)
			errs() << *i->second[j] << "\n";
	}

	errs() << "=== Accesses to rank_ff ===\n";
	for (size_t i = 0; i < accesses_to_ff.size(); ++i)
		errs() << *accesses_to_ff[i] << "\n";

	AliasAnalysis &AA = getAnalysis<AdvancedAlias>();
	// SolveConstraints &SC = getAnalysis<SolveConstraints>();
	for (DenseMap<Function *, ValueList>::iterator
			i1 = accesses_to_me.begin(); i1 != accesses_to_me.end(); ++i1) {
		DenseMap<Function *, ValueList>::iterator i2;
		for (i2 = i1, ++i2; i2 != accesses_to_me.end(); ++i2) {
			for (size_t j1 = 0; j1 < i1->second.size(); ++j1) {
				for (size_t j2 = 0; j2 < i2->second.size(); ++j2) {
					errs() << "{" << i1->first->getName() << ":" << j1 << "} != {" <<
						i2->first->getName() << ":" << j2 << "}? ...";
					assert(AA.alias(i1->second[j1], i2->second[j2]) ==
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

void IntTest::radix(Module &M) {
	TestBanner X("RADIX");

	radix_common(M);

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

void IntTest::radix_common(Module &M) {
	// MyNum's are distinct. 
	vector<Value *> local_ids;
	forall(Module, f, M) {
		if (starts_with(f->getName(), "slave_sort.SLICER")) {
			string str_id = f->getName().substr(strlen("slave_sort.SLICER"));
			int id = (str_id == "" ? 0 : atoi(str_id.c_str()) - 1);
			assert(id >= 0);
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

	Clause *disj = NULL;
	for (size_t i = 0; i < local_ids.size(); ++i) {
		Clause *c = new Clause(new BoolExpr(CmpInst::ICMP_EQ,
					new Expr(local_ids[i]), new Expr(ConstantInt::get(int_type, 1))));
		if (disj)
			disj = new Clause(Instruction::Or, disj, c);
		else
			disj = c;
	}
	assert(disj);

	// local_ids starts from 1. 
	errs() << "At least one of the local_ids is 1? ...";
	assert(SC.provable(disj));
	delete disj;
	print_pass(errs());
}
