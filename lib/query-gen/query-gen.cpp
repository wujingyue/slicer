#include "llvm/Support/CommandLine.h"
#include "common/IDAssigner.h"
using namespace llvm;

#include "slicer/query-gen.h"
#include "slicer/clone-info-manager.h"
using namespace slicer;

static RegisterPass<QueryGenerator> X("gen-query",
		"Generate alias queries from a program",
		false, true);

char QueryGenerator::ID = 0;

bool QueryGenerator::runOnModule(Module &M) {
	CloneInfoManager &CIM = getAnalysis<CloneInfoManager>();

	bool has_clone_info = false;
	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
			for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
				if (CIM.has_clone_info(ins))
					has_clone_info = true;
			}
		}
	}
	assert(has_clone_info && "The program doesn't have any clone info");

	all_stores.clear();
	all_loads.clear();
	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
			for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
				if (StoreInst *si = dyn_cast<StoreInst>(ins))
					all_stores.push_back(si);
				if (LoadInst *li = dyn_cast<LoadInst>(ins))
					all_loads.push_back(li);
			}
		}
	}

	return false;
}

void QueryGenerator::print(raw_ostream &O, const Module *M) const {
	IDAssigner &IDA = getAnalysis<IDAssigner>();

	for (size_t i = 0; i < all_stores.size(); ++i) {
		for (size_t j = i + 1; j < all_stores.size(); ++j) {
			O << IDA.getInstructionID(all_stores[i]) << " " <<
				IDA.getInstructionID(all_stores[j]) << "\n";
		}
	}

	for (size_t i = 0; i < all_stores.size(); ++i) {
		for (size_t j = 0; j < all_loads.size(); ++j) {
			O << IDA.getInstructionID(all_stores[i]) << " " <<
				IDA.getInstructionID(all_loads[j]) << "\n";
		}
	}
}

void QueryGenerator::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<CloneInfoManager>();
	ModulePass::getAnalysisUsage(AU);
}
