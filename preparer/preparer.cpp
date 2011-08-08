/**
 * Author: Jingyue
 */

#include "llvm/Support/Debug.h"
#include "llvm/Support/CFG.h"
#include "common/cfg/identify-thread-funcs.h"
#include "common/include/util.h"
using namespace llvm;

namespace slicer {

	struct Preparer: public ModulePass {

		static char ID;
		Preparer(): ModulePass(&ID) {}
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual bool runOnModule(Module &M);
	};
}
using namespace slicer;

static RegisterPass<Preparer> X(
		"prepare",
		"Adds enforcing landmarks at the entry and exits of all thread functions");

char Preparer::ID = 0;

void Preparer::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesCFG();
	AU.addRequired<IdentifyThreadFuncs>();
	ModulePass::getAnalysisUsage(AU);
}

bool Preparer::runOnModule(Module &M) {

	IdentifyThreadFuncs &ITF = getAnalysis<IdentifyThreadFuncs>();

	FunctionType *pth_self_fty = FunctionType::get(
			IntegerType::get(M.getContext(), __WORDSIZE), false);
	Function *pth_self = M.getFunction("pthread_self");
	if (!pth_self) {
		pth_self = dyn_cast<Function>(
				M.getOrInsertFunction("pthread_self", pth_self_fty));
		pth_self->setDoesNotThrow();
		pth_self->setDoesNotAccessMemory();
	}
	assert(pth_self);
	
	forallfunc(M, fi) {
		if (fi->isDeclaration())
			continue;
		if (ITF.is_thread_func(fi) || is_main(fi)) {
			Instruction *first = fi->getEntryBlock().getFirstNonPHI();
			CallInst::Create(pth_self, "", first);
			forall(Function, bi, *fi) {
				if (succ_begin(bi) == succ_end(bi)) {
					TerminatorInst *ti = bi->getTerminator();
					if (isa<ReturnInst>(ti) || isa<UnwindInst>(ti)) {
						CallInst::Create(pth_self, "", ti);
					}
				}
			}
		}
	}

	return true;
}
