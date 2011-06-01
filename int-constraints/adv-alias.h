/**
 * Author: Jingyue
 */

#ifndef __SLICER_ADV_ALIAS_H
#define __SLICER_ADV_ALIAS_H

#include "llvm/Pass.h"
using namespace llvm;

namespace slicer {

	struct AdvancedAlias: public ModulePass {

		static char ID;

		AdvancedAlias(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void print(raw_ostream &O, const Module *M) const;
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
	};
}

#endif
