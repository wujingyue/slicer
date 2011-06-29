/**
 * Author: Jingyue
 */

#include "llvm/Pass.h"
#include "idm/id.h"
#include "common/cfg/partial-icfg-builder.h"
using namespace llvm;

namespace slicer {

	struct ICFGTest: public ModulePass {

		static char ID;

		ICFGTest(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;
	};
}
using namespace slicer;

static RegisterPass<ICFGTest> X(
		"icfg-test",
		"Dumps the ICFG of the program",
		false,
		true);

char ICFGTest::ID = 0;

void ICFGTest::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<ObjectID>();
	AU.addRequired<PartialICFGBuilder>();
	ModulePass::getAnalysisUsage(AU);
}

bool ICFGTest::runOnModule(Module &M) {
	return false;
}

void ICFGTest::print(raw_ostream &O, const Module *M) const {
	ICFG &icfg = getAnalysis<PartialICFGBuilder>();
	icfg.print(O, getAnalysis<ObjectID>());
}
