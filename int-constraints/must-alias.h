#ifndef __SLICER_MUST_ALIAS_H
#define __SLICER_MUST_ALIAS_H

// TODO: fit into AliasAnalysis interface. 

#include "llvm/Module.h"
using namespace llvm;

namespace slicer {
	
	struct MustAlias: public ModulePass {

		static char ID;

		MustAlias(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module &M) const;

		bool must_alias(const Value *v1, const Value *v2);

	private:

	};
}

#endif
