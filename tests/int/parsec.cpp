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
