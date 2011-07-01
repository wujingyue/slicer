/**
 * Author: Jingyue
 */

#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "idm/id.h"
#include "idm/mbb.h"
#include "common/cfg/partial-icfg-builder.h"
#include "common/cfg/reach.h"
using namespace llvm;

#include "../include/test-banner.h"

namespace slicer {

	struct ICFGTest: public ModulePass {

		static char ID;

		ICFGTest(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;

	private:
		void test_test_overwrite(Module &M);
	};
}
using namespace slicer;

static RegisterPass<ICFGTest> X(
		"icfg-test",
		"Dumps the ICFG of the program",
		false,
		true);

static cl::opt<string> Program(
		"prog",
		cl::desc("The program being tested (e.g. aget). "
			"Usually it's simply the name of the bc file without \".bc\"."),
		cl::init(""));

char ICFGTest::ID = 0;

void ICFGTest::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<ObjectID>();
	AU.addRequired<MicroBasicBlockBuilder>();
	AU.addRequired<PartialICFGBuilder>();
	ModulePass::getAnalysisUsage(AU);
}

bool ICFGTest::runOnModule(Module &M) {
	test_test_overwrite(M);
	return false;
}

void ICFGTest::test_test_overwrite(Module &M) {
	
	if (Program != "test-overwrite")
		return;
	TestBanner X("test-overwrite");

	const Instruction *i1 = NULL, *i2 = NULL;
	forallinst(M, ins) {
		if (const LoadInst *li = dyn_cast<LoadInst>(ins)) {
			if (li->isVolatile()) {
				if (i1 == NULL)
					i1 = li;
				else if (i2 == NULL)
					i2 = li;
				else
					assert(false && "There should be exactly 2 volatile loads.");
			}
		}
	}
	assert(i1 && i2 && "There should be exactly 2 volatile loads.");
	if (i1->getParent()->getParent()->getName() != "main")
		swap(i1, i2);

	MicroBasicBlockBuilder &MBBB = getAnalysis<MicroBasicBlockBuilder>();
	const MicroBasicBlock *m1 = MBBB.parent(i1), *m2 = MBBB.parent(i2);
	
	ICFG &PIB = getAnalysis<PartialICFGBuilder>();
	ICFGNode *n1 = PIB[m1], *n2 = PIB[m2];
	assert(n1 && n2);

	Reach<ICFGNode> R;
	errs() << *i1 << "\n" << *i2 << "\n";
	assert(R.reachable(n1, n2));
}

void ICFGTest::print(raw_ostream &O, const Module *M) const {
	ICFG &icfg = getAnalysis<PartialICFGBuilder>();
	icfg.print(O, getAnalysis<ObjectID>());
}
