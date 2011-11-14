/*
 * TODO: should be moved out of Trace because other parts need it as well. 
 * However, TernLandmarks.a is broken now. We'll move it after we can have
 * a reliable and clean module to test whether an instruction is a Tern
 * landmark.
 */

#ifndef __SLICER_OMIT_BRANCH_H
#define __SLICER_OMIT_BRANCH_H

#include "llvm/Module.h"
#include "llvm/Pass.h"

using namespace llvm;
using namespace std;

namespace slicer {
	struct OmitBranch: public ModulePass {
		static char ID;

		OmitBranch(): ModulePass(ID) {}

		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual bool runOnModule(Module &M);
		virtual void print(raw_ostream &O, const Module *M) const;
		bool omit(TerminatorInst *ti);

	private:
		void dfs(BasicBlock *x, BasicBlock *sink);
		
		DenseMap<BasicBlock *, bool> visited;
	};
}

#endif
