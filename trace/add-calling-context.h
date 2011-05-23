#ifndef __TERN_ADD_CALLING_CONTEXT_H
#define __TERN_ADD_CALLING_CONTEXT_H

#include "llvm/Pass.h"
#include "llvm/Module.h"
using namespace llvm;

#include <map>
#include <vector>
using namespace std;

namespace slicer {
	
	typedef vector<unsigned> CallStack;

	struct AddCallingContext: public ModulePass {

		static char ID;

		AddCallingContext(): ModulePass(&ID) {}

		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;
		const CallStack &get_calling_context(unsigned idx) const;

	private:
		static bool is_func_entry(const Instruction *ins);

		// TODO: Needs a lot of space.
		// Can we compute calling contexts on demand? 
		vector<CallStack> contexts;
	};
}

#endif

