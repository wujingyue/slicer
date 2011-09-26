#ifndef __SLICER_QUERY_DRIVER_H
#define __SLICER_QUERY_DRIVER_H

#include <vector>
using namespace std;

#include "llvm/Pass.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "common/typedefs.h"
using namespace llvm;

namespace slicer {
	struct QueryDriver: public ModulePass {
		static char ID;
		
		QueryDriver(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;

	private:
		void read_queries();
		void issue_queries();
		
		vector<ConstInstPair> queries;
		vector<AliasAnalysis::AliasResult> results;
	};
}

#endif
