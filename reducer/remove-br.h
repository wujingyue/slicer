/**
 * Author: Jingyue
 */

#ifndef __SLICER_REMOVE_BR_H
#define __SLICER_REMOVE_BR_H

#include "llvm/Pass.h"
#include "llvm/Instructions.h"
using namespace llvm;

#include <vector>
using namespace std;

namespace slicer {

	struct RemoveBranch: public ModulePass {

		static char ID;

		RemoveBranch(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;

	private:
		/*
		 * Check whether we are able to remove any branch of this BranchInst.
		 * Add those removable branches to <to_remove>. 
		 */
		void prepare_remove_branch(
				BranchInst *bi, vector<pair<BranchInst *, unsigned> > &to_remove);
		/*
		 * This function does nothing if bi->getSuccessor(i) is already an
		 * unreachable BB. It returns <false> in that case. 
		 */
		bool remove_branch(
				TerminatorInst *bi, unsigned i, BasicBlock *&unreachable_bb);
	};
}

#endif
