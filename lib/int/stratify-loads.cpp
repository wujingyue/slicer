/**
 * Author: Jingyue
 */

#define DEBUG_TYPE "int"

#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "common/InitializePasses.h"
#include "slicer/InitializePasses.h"
using namespace llvm;

#include "common/callgraph-fp.h"
#include "common/exec-once.h"
using namespace rcs;

namespace slicer {
	struct StratifyLoads: public ModulePass {
		static char ID;
		StratifyLoads();
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;
		virtual bool runOnModule(Module &M);
	};
}
using namespace slicer;

INITIALIZE_PASS_BEGIN(StratifyLoads, "stratify-loads",
		"Stratify load instructions", false, true)
INITIALIZE_PASS_DEPENDENCY(ExecOnce)
INITIALIZE_PASS_DEPENDENCY(CallGraphFP)
INITIALIZE_PASS_END(StratifyLoads, "stratify-loads",
		"Stratify load instructions", false, true)

void StratifyLoads::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<ExecOnce>();
	AU.addRequired<CallGraphFP>();
}

char StratifyLoads::ID = 0;

StratifyLoads::StratifyLoads(): ModulePass(ID) {
	initializeStratifyLoadsPass(*PassRegistry::getPassRegistry());
}

bool StratifyLoads::runOnModule(Module &M) {
	return false;
}

void StratifyLoads::print(raw_ostream &O, const Module *M) const {
}
