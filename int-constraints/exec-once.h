/**
 * Author: Jingyue
 *
 * Decide whether an instruction can be executed only once (or never).
 */

#ifndef __SLICER_EXEC_ONCE_H
#define __SLICER_EXEC_ONCE_H

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "common/include/typedefs.h"
using namespace llvm;

namespace slicer {

	struct ExecOnce: public ModulePass {

		static char ID;

		ExecOnce(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;

		/* Executed only once? */
		bool executed_once(const Instruction *ins) const;
		bool executed_once(const BasicBlock *bb) const;
		bool executed_once(const Function *func) const;

	private:
		// Used in <identify_twice_funcs>. 
		void identify_starting_funcs(Module &M, FuncSet &starts);
		// Identify all functions that can be executed more than once. 
		void identify_twice_funcs(Module &M);
		// Identify all BBs in loops. 
		// Note that this set isn't equivalent to all the BBs that can be
		// executed more than once. 
		void identify_twice_bbs(Module &M);
		// Used in <identify_twice_funcs>.
		// DFS from a starting function via the call graph. 
		void propagate_via_cg(Function *f);

		// Results of function <identify_twice_funcs>.
		FuncSet twice_funcs;
		// Results of function <identify_twice_bbs>.
		BBSet twice_bbs;
	};
}

#endif
