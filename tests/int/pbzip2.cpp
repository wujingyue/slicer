#include "common/typedefs.h"
using namespace rcs;

#include "slicer/adv-alias.h"
#include "slicer/test-utils.h"
#include "int-test.h"
using namespace slicer;

void IntTest::pbzip2_like(Module &M) {
	TestBanner X("pbzip2-like");

	vector<StoreInst *> writes;
	Function *f_rand = M.getFunction("rand");
	assert(f_rand);
	Function *f_producer = M.getFunction("_Z8producerPv.SLICER");
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
	Function *f_consumer = M.getFunction("_Z8consumerPv.SLICER");
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
