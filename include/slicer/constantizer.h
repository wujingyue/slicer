/**
 * Author: Jingyue
 *
 * We also replace variables with constants if their values are know by
 * querying the solver. 
 */

#ifndef __SLICER_CONSTANTIZER_H
#define __SLICER_CONSTANTIZER_H

#include "llvm/Pass.h"
#include "llvm/Instructions.h"
#include "llvm/ADT/DenseMap.h"
using namespace llvm;

#include <vector>
using namespace std;

namespace slicer {
	struct Constantizer: public ModulePass {
		static char ID;

		Constantizer(): ModulePass(ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		
	private:
		/** Replace variables with ConstantInts whenever possible. */
		bool constantize(Module &M);
		void setup(Module &M);
		Function *get_slicer_assert(Module &M, const Type *type);

	private:
		DenseMap<const Type *, Function *> slicer_assert_eq;
	};
}

#endif
