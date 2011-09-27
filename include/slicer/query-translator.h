#ifndef __SLICER_QUERY_TRANSLATOR_H
#define __SLICER_QUERY_TRANSLATOR_H

#include "llvm/Pass.h"
#include "common/typedefs.h"

namespace slicer {
	struct QueryTranslator: public ModulePass {
		static char ID;
		
		QueryTranslator(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
	};
}

#endif
