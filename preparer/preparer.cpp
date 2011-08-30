/**
 * Author: Jingyue
 *
 * Adds enforcing landmarks at the entry and exits of all thread functions
 * including function main. 
 */

#include "llvm/Support/Debug.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/CommandLine.h"
#include "common/cfg/identify-thread-funcs.h"
#include "common/include/util.h"
using namespace llvm;

namespace slicer {
	struct Preparer: public ModulePass {
		static char ID;

		Preparer(): ModulePass(&ID) {}
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual bool runOnModule(Module &M);

	private:
		bool is_specified_thread_function(const Function *f) const;
	};
}
using namespace slicer;

static RegisterPass<Preparer> X("prepare",
		"Adds enforcing landmarks at the entry and exits of all thread functions");

static cl::list<string> OtherThreadFunctions("thread-func",
		cl::desc("Other functions that should be treated as thread functions, "
			"e.g. child_main in Apache"));

char Preparer::ID = 0;

void Preparer::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesCFG();
	AU.addRequired<IdentifyThreadFuncs>();
	ModulePass::getAnalysisUsage(AU);
}

bool Preparer::is_specified_thread_function(const Function *f) const {
	for (cl::list<string>::const_iterator itr = OtherThreadFunctions.begin();
			itr != OtherThreadFunctions.end(); ++itr) {
		if (f->getName() == *itr)
			return true;
	}
	return false;
}

bool Preparer::runOnModule(Module &M) {
	IdentifyThreadFuncs &ITF = getAnalysis<IdentifyThreadFuncs>();

	for (cl::list<string>::const_iterator itr = OtherThreadFunctions.begin();
			itr != OtherThreadFunctions.end(); ++itr) {
		DEBUG(dbgs() << "Other thread functions: " << *itr << "\n";);
	}

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
	
	forallfunc(M, f) {
		if (f->isDeclaration())
			continue;
		// Needn't instrument entries and exits of function main,
		// because nobody calls main and thus no ambiguity. 
		if (ITF.is_thread_func(f) || is_main(f) ||
				is_specified_thread_function(f)) {
			Instruction *first = f->getEntryBlock().getFirstNonPHI();
			CallInst::Create(pth_self, "", first);
			forall(Function, bi, *f) {
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
