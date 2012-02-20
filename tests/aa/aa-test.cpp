/**
 * Author: Jingyue
 */

#include "llvm/Support/CommandLine.h"
using namespace llvm;

#include "common/IDAssigner.h"
using namespace rcs;

#include "bc2bdd/BddAliasAnalysis.h"
using namespace bc2bdd;

#include "slicer/adv-alias.h"
#include "slicer/iterate.h"

namespace slicer {
	struct AATest: public ModulePass {
		static char ID;

		AATest(): ModulePass(ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
	};
}
using namespace slicer;

static RegisterPass<AATest> X("aa-test",
		"Test alias analysis", false, true);
static cl::opt<bool> UseAdvancedAA("use-adv-aa",
		cl::desc("Use the advanced AA if turned on"));
static cl::opt<bool> ValueID("value",
		cl::desc("Use value IDs instead of instruction IDs"));
static cl::opt<unsigned> ID1("id1", cl::desc("the first ID"),
		cl::init(IDAssigner::INVALID_ID));
static cl::opt<unsigned> ID2("id2", cl::desc("the second ID"),
		cl::init(IDAssigner::INVALID_ID));

char AATest::ID = 0;

void AATest::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<IDAssigner>();
	if (UseAdvancedAA) {
		AU.addRequired<Iterate>();
		AU.addRequired<AdvancedAlias>();
	} else {
		AU.addRequired<BddAliasAnalysis>();
	}
}

bool AATest::runOnModule(Module &M) {
	IDAssigner &IDA = getAnalysis<IDAssigner>();
	AliasAnalysis *AA = NULL;
	if (UseAdvancedAA)
		AA = &getAnalysis<AdvancedAlias>();
	else
		AA = &getAnalysis<BddAliasAnalysis>();

	const Value *v1 = NULL, *v2 = NULL;
	if (ValueID) {
		v1 = IDA.getValue(ID1);
		v2 = IDA.getValue(ID2);
	} else {
		Instruction *i1 = IDA.getInstruction(ID1);
		Instruction *i2 = IDA.getInstruction(ID2);
		assert(i1 && i2);
		assert(isa<StoreInst>(i1) || isa<LoadInst>(i1));
		assert(isa<StoreInst>(i2) || isa<LoadInst>(i2));
		if (isa<StoreInst>(i1))
			v1 = cast<StoreInst>(i1)->getPointerOperand();
		else
			v1 = cast<LoadInst>(i1)->getPointerOperand();
		if (isa<StoreInst>(i2))
			v2 = cast<StoreInst>(i2)->getPointerOperand();
		else
			v2 = cast<LoadInst>(i2)->getPointerOperand();
	}
	assert(v1 && v2);

	errs() << *v1 << "\n" << *v2 << "\n";
	errs() << AA->alias(v1, v2) << "\n";
	return false;
}
