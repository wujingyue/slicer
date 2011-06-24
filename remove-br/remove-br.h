#ifndef __SLICER_REMOVE_BR_H
#define __SLICER_REMOVE_BR_H

#include "llvm/Pass.h"
#include "llvm/Instructions.h"
using namespace llvm;

namespace slicer {

	struct RemoveBranch: public ModulePass {

		static char ID;

		RemoveBranch(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;

	private:
		/*
		 * We remove a branch by redirecting it to the unreachable BB of
		 * the containing function. Once the unreachable BB is created, we can
		 * reuse it later. 
		 * 
		 * Returns whether we've changed <bi>. 
		 */
		bool try_remove_branch(BranchInst *bi, BasicBlock *&unreachable_bb);
		void remove_branch(
				TerminatorInst *bi, unsigned i, BasicBlock *&unreachable_bb);
	};
}

#endif
