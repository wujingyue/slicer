#ifndef __SLICER_ADD_CALLING_CONTEXT_H
#define __SLICER_ADD_CALLING_CONTEXT_H

#include <vector>
using namespace std;

#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "common/typedefs.h"
using namespace llvm;

namespace slicer {
	struct AddCallingContext: public ModulePass {
		static char ID;

		AddCallingContext(): ModulePass(ID) {}

		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;
		ConstInstList get_calling_context(unsigned idx) const;

	private:
		// TODO: Needs a lot of space.
		// Can we compute calling contexts on demand? 
		vector<ConstInstList> contexts;
	};
}

#endif
