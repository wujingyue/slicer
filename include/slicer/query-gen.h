#ifndef __SLICER_QUERY_GEN_H
#define __SLICER_QUERY_GEN_H

#include <vector>
using namespace std;

#include "llvm/Instructions.h"
#include "llvm/Pass.h"
using namespace llvm;

namespace slicer {
	struct QueryGenerator: public ModulePass {
		static char ID;

		QueryGenerator(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;

	private:
		vector<StoreInst *> all_stores;
		vector<LoadInst *> all_loads;
	};
}

#endif
