/**
 * Author: Jingyue
 */

#include "llvm/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#include "bc2bdd/BddAliasAnalysis.h"
using namespace bc2bdd;

#include "common/typedefs.h"
using namespace rcs;

#include "slicer/adv-alias.h"
#include "slicer/test-utils.h"
#include "slicer/solve.h"
#include "int-test.h"
using namespace slicer;

void IntTest::blackscholes(Module &M) {
	AliasAnalysis &AA = getAnalysis<AdvancedAlias>();

	Value *prices = M.getNamedGlobal("prices");
	assert(prices);

	InstList loads;
	for (Value::use_iterator ui = prices->use_begin();
			ui != prices->use_end(); ++ui) {
		if (LoadInst *li = dyn_cast<LoadInst>(*ui)) {
			Function *f = li->getParent()->getParent();
			if (f->getName().startswith("_Z9bs_threadPv.SLICER"))
				loads.push_back(li);
		}
	}

	InstList geps;
	for (size_t i = 0; i < loads.size(); ++i) {
		Instruction *li = loads[i];
		for (Value::use_iterator ui = li->use_begin(); ui != li->use_end(); ++ui) {
			if (GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(*ui))
				geps.push_back(gep);
		}
	}

	dbgs() << "# of geps = " << geps.size() << "\n";
	for (size_t i = 0; i < geps.size(); ++i) {
		for (size_t j = i + 1; j < geps.size(); ++j) {
			errs() << "Gep " << i << " != Gep " << j << "? ...";
			AliasAnalysis::AliasResult res = AA.alias(geps[i], geps[j]);
			assert(res == AliasAnalysis::NoAlias);
			print_pass(errs());
		}
	}
}

void IntTest::raytrace_like(Module &M) {
	TestBanner X("raytrace-like");

	DenseMap<Function *, InstList> writes;
	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		if (f->getName().find("task") != string::npos &&
				f->getName().find(".SLICER") != string::npos) {
			for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
				for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
					if (StoreInst *si = dyn_cast<StoreInst>(ins)) {
						if (isa<LoadInst>(si->getValueOperand()))
							writes[f].push_back(si);
					}
				}
			}
		}
	}

	size_t n_writes = 0;
	for (DenseMap<Function *, InstList>::iterator i = writes.begin();
			i != writes.end(); ++i) {
		n_writes += i->second.size();
	}
	errs() << "# of writes = " << n_writes << "\n";

	AliasAnalysis &AA = getAnalysis<AdvancedAlias>();
	DenseMap<Function *, InstList>::iterator i1, i2;
	for (i1 = writes.begin(); i1 != writes.end(); ++i1) {
		i2 = i1;
		for (++i2; i2 != writes.end(); ++i2) {
			for (size_t j1 = 0; j1 < min(4UL, i1->second.size()); ++j1) {
				for (size_t j2 = 0; j2 < min(4UL, i2->second.size()); ++j2) {
					errs() << "Store: {" << i1->first->getName() << ":" << j1 <<
						"} and {" << i2->first->getName() << ":" << j2 <<
						"} are disjoint? ...";
					Instruction *ins1 = i1->second[j1];
					Instruction *ins2 = i2->second[j2];
					StoreInst *s1 = dyn_cast<StoreInst>(ins1);
					StoreInst *s2 = dyn_cast<StoreInst>(ins2);
					AliasAnalysis::AliasResult res = AA.alias(
							s1->getPointerOperand(),
							s2->getPointerOperand());
					assert(res == AliasAnalysis::NoAlias);
					print_pass(errs());
				}
			}
		}
	}
}

void IntTest::ferret_like(Module &M) {
	TestBanner X("ferret-like");

	vector<StoreInst *> writes;
	Function *f_rand = M.getFunction("rand");
	assert(f_rand);
	Function *f_producer = M.getFunction("producer.SLICER");
	assert(f_producer);
	// Search along the CFG. We need to make sure reads and writes are in
	// a consistent order. 
	for (Function::iterator bb = f_producer->begin();
			bb != f_producer->end(); ++bb) {
		for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
			if (CallInst *ci = dyn_cast<CallInst>(ins)) {
				if (ci->getCalledFunction() == f_rand) {
					for (BasicBlock::iterator j = bb->begin(); j != bb->end(); ++j) {
						if (StoreInst *si = dyn_cast<StoreInst>(j))
							writes.push_back(si);
					}
				}
			}
		}
	}
	errs() << "=== writes ===\n";
	for (size_t i = 0; i < writes.size(); ++i) {
		errs() << *writes[i] << "\n";
	}

	vector<LoadInst *> reads;
	Function *f_consumer = M.getFunction("consumer.SLICER");
	assert(f_consumer);
	for (Function::iterator bb = f_consumer->begin();
			bb != f_consumer->end(); ++bb) {
		for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
			if (ins->getOpcode() == Instruction::Add &&
					ins->getType()->isIntegerTy(8)) {
				LoadInst *li = dyn_cast<LoadInst>(ins->getOperand(0));
				assert(li);
				reads.push_back(li);
			}
		}
	}
	errs() << "=== reads ===\n";
	for (size_t i = 0; i < reads.size(); ++i) {
		errs() << *reads[i] << "\n";
	}

	assert(writes.size() == reads.size());
	AliasAnalysis &AA = getAnalysis<AdvancedAlias>();
	// AliasAnalysis &AA = getAnalysis<BddAliasAnalysis>();
	for (size_t i = 0; i < writes.size(); ++i) {
		for (size_t j = i + 1; j < reads.size(); ++j) {
			errs() << "i = " << i << ", j = " << j << "... ";
			AliasAnalysis::AliasResult res = AA.alias(
					writes[i]->getPointerOperand(),
					reads[j]->getPointerOperand());
			assert(res == AliasAnalysis::NoAlias);
			print_pass(errs());
		}
	}
}

void IntTest::bodytrack_like(Module &M) {
	TestBanner X("bodytrack-like");

	DenseMap<Function *, vector<StoreInst *> > writes;
	Function *f_rand = M.getFunction("rand");
	assert(f_rand);

	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		if (f->getName().find("thread_entry") != string::npos &&
				f->getName().find(".SLICER") != string::npos) {
			for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
				for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
					if (CallInst *ci = dyn_cast<CallInst>(ins)) {
						if (ci->getCalledFunction() == f_rand) {
							for (Value::use_iterator ui = ci->use_begin();
									ui != ci->use_end(); ++ui) {
								assert(isa<StoreInst>(*ui));
								writes[f].push_back(cast<StoreInst>(*ui));
							}
						}
					}
				}
			}
		}
	}

	errs() << "# of thread functions = " << writes.size() << "\n";

	AliasAnalysis &AA = getAnalysis<AdvancedAlias>();
	SolveConstraints &SC = getAnalysis<SolveConstraints>();
	DenseMap<Function *, vector<StoreInst *> >::iterator i1, i2;
	for (i1 = writes.begin(); i1 != writes.end(); ++i1) {
		i2 = i1;
		for (++i2; i2 != writes.end(); ++i2) {
			for (size_t j1 = 0; j1 < min(4UL, i1->second.size()); ++j1) {
				for (size_t j2 = 0; j2 < min(4UL, i2->second.size()); ++j2) {
					errs() << "Store: {" << i1->first->getName() << ":" << j1 <<
						"} and {" << i2->first->getName() << ":" << j2 <<
						"} are disjoint? ...";
					StoreInst *s1 = i1->second[j1];
					StoreInst *s2 = i2->second[j2];
					SC.set_print_counterexample(true);
					AliasAnalysis::AliasResult res = AA.alias(
							s1->getPointerOperand(),
							s2->getPointerOperand());
					SC.set_print_counterexample(false);
					assert(res == AliasAnalysis::NoAlias);
					print_pass(errs());
				}
			}
		}
	}
}
