/**
 * Author: Jingyue
 */

#ifndef __SLICER_PATH_COUNTER_H
#define __SLICER_PATH_COUNTER_H

#include "llvm/Pass.h"
using namespace llvm;

namespace slicer {
	struct PathCounter: public ModulePass {
		struct Edge {
			Edge(BasicBlock *y, bool intra): target(y), intra_procedural(intra) {}
			BasicBlock *target;
			bool intra_procedural;
		};

		static char ID;

		PathCounter(): ModulePass(ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;

	private:
		void dfs(BasicBlock *x, unsigned &cur_time);
		long double compute_num_paths(BasicBlock *x);

		DenseMap<BasicBlock *, vector<Edge> > g;
		DenseMap<BasicBlock *, unsigned> start_time, finish_time;
		DenseMap<BasicBlock *, long double> n_paths;
	};
}

#endif
