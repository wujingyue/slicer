#ifndef __SLICER_QUERY_DRIVER_H
#define __SLICER_QUERY_DRIVER_H

#include <vector>
using namespace std;

#include "llvm/Pass.h"
#include "llvm/Analysis/AliasAnalysis.h"
using namespace llvm;

#include "rcs/typedefs.h"
using namespace rcs;

namespace slicer {
	struct ContextedIns {
		const Instruction *ins;
		ConstInstList callstack;
	};

	struct QueryDriver: public ModulePass {
		static char ID;

		QueryDriver();
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;

	private:
		void read_queries();
		void issue_queries();
		void parse_contexted_ins(const string &str, ContextedIns &ci);

		vector<pair<ContextedIns, ContextedIns> > queries;
		vector<AliasAnalysis::AliasResult> results;

		double total_time;
	};
}

#endif
