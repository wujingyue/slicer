/**
 * Author: Jingyue
 *
 * Annotate each instruction in the sliced part with its previous
 * enforcing landmark and its next enforcing landmark. 
 *
 * A region is a set of contiguous trunks bounded by enforcing landmarks. 
 */

#ifndef __SLICER_REGION_BUILDER_H
#define __SLICER_REGION_BUILDER_H

#include "llvm/Pass.h"
using namespace llvm;

namespace slicer {

	struct RegionBuilder: public ModulePass {

		static char ID;

		RegionBuilder(): ModulePass(&ID) {}
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual bool runOnModule(Module &M);

	private:
		void mark_region(
				Instruction *s_ins, Instruction *e_ins,
				int thr_id, size_t s_tr, size_t e_tr);
	};
}

#endif
