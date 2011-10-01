/**
 * Author: Jingyue
 *
 * If a conditional branch is guaranteed to be true/false, we redirect its
 * false/true branch to an unreachable BB. This simplification may make
 * some BBs unreachable, and thus eliminate them later. 
 *
 * We also replace variables with constants if their values are know by
 * querying the solver. 
 */

#ifndef __SLICER_POST_REDUCER_H
#define __SLICER_POST_REDUCER_H

#include "llvm/Pass.h"
#include "llvm/Instructions.h"
#include "llvm/ADT/DenseMap.h"
using namespace llvm;

#include <vector>
using namespace std;

namespace slicer {
	struct PostReducer: public ModulePass {
		static char ID;

		PostReducer(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		
	private:
		/**
		 * Check whether we are able to remove any branch of this BranchInst.
		 * Add those removable branches to <to_remove>. 
		 */
		void prepare_remove_branch(BranchInst *bi,
				vector<pair<BranchInst *, unsigned> > &to_remove);
		/**
		 * This function does nothing if bi->getSuccessor(i) is already an
		 * unreachable BB. It returns <false> in that case. 
		 */
		bool remove_branch(TerminatorInst *bi, unsigned i,
				BasicBlock *&unreachable_bb);
		/** Remove unreachable branches. */
		bool remove_branches(Module &M);
		/** Replace variables with ConstantInts whenever possible. */
		bool constantize(Module &M);
		void setup(Module &M);
		void setup_slicer_assert_eq(Module &M, unsigned bit_width);

	private:
		// How ugly it is!
		DenseMap<unsigned, Constant *> slicer_assert_eq;
	};
}

#endif
