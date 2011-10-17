/**
 * Author: Jingyue
 */

#define DEBUG_TYPE "metrics"

#include "llvm/ADT/Statistic.h"
#include "common/IDManager.h"
#include "common/exec-once.h"
using namespace llvm;

#include "slicer/clone-info-manager.h"
#include "inst-counter.h"
using namespace slicer;

char InstCounter::ID = 0;

static RegisterPass<InstCounter> X("count-insts",
		"Count number of stores and loads",
		false, true);

STATISTIC(NumStores, "Number of stores");
STATISTIC(NumLoads, "Number of loads");

void InstCounter::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<IDManager>();
	AU.addRequired<CloneInfoManager>();
	AU.addRequired<ExecOnce>();
	ModulePass::getAnalysisUsage(AU);
}

bool InstCounter::runOnModule(Module &M) {
	IDManager &IDM = getAnalysis<IDManager>();
	CloneInfoManager &CIM = getAnalysis<CloneInfoManager>();
	ExecOnce &EO = getAnalysis<ExecOnce>();
	assert(CIM.has_clone_info());

	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
			for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
				if (EO.not_executed(ins))
					continue;
				if (isa<LoadInst>(ins) || isa<StoreInst>(ins)) {
					unsigned ins_id = IDM.getInstructionID(ins);
					if (ins_id != IDManager::INVALID_ID) {
						count_inst(ins, ins_id);
					} else if (CIM.has_clone_info(ins)) {
						CloneInfo ci = CIM.get_clone_info(ins);
						count_inst(ins, ci.orig_ins_id);
					}
				}
			}
		}
	}

	return false;
}

void InstCounter::count_inst(const Instruction *ins, unsigned ins_id) {
	if (counted_ins_ids.count(ins_id))
		return;
	counted_ins_ids.insert(ins_id);
	if (isa<StoreInst>(ins))
		++NumStores;
	else if (isa<LoadInst>(ins))
		++NumLoads;
}

void InstCounter::print(raw_ostream &O, const Module *M) const {
}
