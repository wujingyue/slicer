/**
 * Author: Jingyue
 *
 * Check whether the landmark trace is valid. Currently, very limited checks:
 * 1. Executed instructions are not marked as "not executed" by ExecOnce.
 */

#define DEBUG_TYPE "trace"

#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#include "common/exec-once.h"
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

INITIALIZE_PASS_BEGIN(ValidityChecker, "check-validity",
		"Validity checker", false, true)
INITIALIZE_PASS_DEPENDENCY(TraceManager)
INITIALIZE_PASS_DEPENDENCY(ExecOnce)
INITIALIZE_PASS_END(ValidityChecker, "check-validity",
		"Validity checker", false, true)

ValidityChecker::ValidityChecker(): ModulePass(ID) {
	initializeValidityCheckerPass(*PassRegistry::getPassRegistry());
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
