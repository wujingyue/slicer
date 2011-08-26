/**
 * Author: Jingyue
 *
 * We replace variables with constants if their values are know by
 * querying the solver. 
 */

#ifndef __SLICER_CONSTANTIZER_H
#define __SLICER_CONSTANTIZER_H

#include <vector>
using namespace std;

#include "llvm/Pass.h"
#include "llvm/Instructions.h"
using namespace llvm;

namespace slicer {
	struct Constantizer: public ModulePass {
		static char ID;

		Constantizer(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;

	private:
		bool constantize(Module &M);
	};
}

#endif
