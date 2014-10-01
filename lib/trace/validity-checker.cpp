/**
 * Author: Jingyue
 *
 * Check whether the landmark trace is valid. Currently, very limited checks:
 * 1. Executed instructions are not marked as "not executed" by ExecOnce.
 */

#define DEBUG_TYPE "trace"

#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#include "rcs/ExecOnce.h"
using namespace rcs;

#include "slicer/trace-manager.h"
using namespace slicer;

namespace slicer {
	struct ValidityChecker: public ModulePass {
		static char ID;

		ValidityChecker();
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
	};
}

ValidityChecker::ValidityChecker(): ModulePass(ID) {
}

void ValidityChecker::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<TraceManager>();
	AU.addRequired<ExecOnce>();
}

char ValidityChecker::ID = 0;

bool ValidityChecker::runOnModule(Module &M) {
	TraceManager &TM = getAnalysis<TraceManager>();
	ExecOnce &EO = getAnalysis<ExecOnce>();

	for (unsigned i = 0; i < TM.get_num_records(); ++i) {
		const TraceRecordInfo &info = TM.get_record_info(i);
		errs() << "!!!!!\n";
		if (EO.not_executed(info.ins))
			errs() << "[Warning] executed:" << *info.ins << "\n";
	}

	return false;
}
