/**
 * Author: Jingyue
 *
 * Adds enforcing landmarks at the entry and exits of all thread functions
 * including function main. 
 */

#include "llvm/PassRegistry.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/CommandLine.h"
#include "common/InitializePasses.h"
#include "slicer/InitializePasses.h"
using namespace llvm;

#include "common/identify-thread-funcs.h"
#include "common/util.h"
using namespace rcs;

#include "slicer/enforcing-landmarks.h"
namespace slicer {
	struct Preparer: public ModulePass {
		static char ID;

		Preparer(): ModulePass(ID) {
			initializePreparerPass(*PassRegistry::getPassRegistry());
		}
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual bool runOnModule(Module &M);

	private:
		bool is_specified_thread_function(const Function *f) const;
	};
}
using namespace slicer;

INITIALIZE_PASS_BEGIN(Preparer, "prepare",
		"Adds enforcing landmarks at the entry and exits of all thread functions",
		false, false)
INITIALIZE_PASS_DEPENDENCY(IdentifyThreadFuncs)
INITIALIZE_PASS_DEPENDENCY(EnforcingLandmarks)
INITIALIZE_PASS_END(Preparer, "prepare",
		"Adds enforcing landmarks at the entry and exits of all thread functions",
		false, false)

void Preparer::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesCFG();
	AU.addRequired<IdentifyThreadFuncs>();
	AU.addRequired<EnforcingLandmarks>();
}

static cl::list<string> OtherThreadFunctions("thread-func",
		cl::desc("Other functions that should be treated as thread functions, "
			"e.g. child_main in Apache"));

char Preparer::ID = 0;

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
	
	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		if (f->isDeclaration())
			continue;

		// Needn't instrument entries and exits of function main,
		// because nobody calls main and thus no ambiguity. 
		if (ITF.is_thread_func(f) || is_main(f) ||
				is_specified_thread_function(f)) {
			Instruction *first = f->getEntryBlock().getFirstNonPHI();
			CallInst::Create(pth_self, "", first);
			for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
				if (succ_begin(bb) == succ_end(bb)) {
					TerminatorInst *ti = bb->getTerminator();
					if (isa<ReturnInst>(ti) || isa<UnwindInst>(ti)) {
						CallInst::Create(pth_self, "", ti);
					}
				}
			}
		}
	}

	// Insert a fake pthread_self call before each blocking function,
	// so that we can correctly reason about sync operations like
	// <pthread_barrier_wait>. 
	EnforcingLandmarks &EL = getAnalysis<EnforcingLandmarks>();
	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
			for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
				if (EL.is_blocking_enforcing_landmark(ins))
					CallInst::Create(pth_self, "", ins);
				// <ins> is still valid. 
			}
		}
	}

	return true;
}

struct RegisterPreparerPasses {
	RegisterPreparerPasses() {
		PassRegistry &reg = *PassRegistry::getPassRegistry();
		initializePreparerPass(reg);
	}
};
static RegisterPreparerPasses X;
