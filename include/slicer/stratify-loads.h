/**
 * Author: Jingyue
 */

#ifndef __SLICER_STRATIFY_LOADS_H
#define __SLICER_STRATIFY_LOADS_H

#include "llvm/Pass.h"
using namespace llvm;

namespace slicer {
	struct StratifyLoads: public ModulePass {
		static char ID;

		StratifyLoads();
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;
		virtual bool runOnModule(Module &M);

		unsigned get_level(const Value *v) const;
		unsigned get_max_level() const;

	private:
		bool try_calculating_levels(Module &M);
		bool is_memory_allocation(Instruction *ins);

		DenseMap<Value *, unsigned> level;
	};
}

#endif
