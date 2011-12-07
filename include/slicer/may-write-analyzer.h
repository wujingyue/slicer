/**
 * Author: Jingyue
 */

#ifndef __SLICER_MAY_WRITE_ANALYZER
#define __SLICER_MAY_WRITE_ANALYZER

#include "llvm/Pass.h"
#include "llvm/Analysis/AliasAnalysis.h"
using namespace llvm;

#include "common/typedefs.h"
using namespace rcs;

namespace slicer {
	struct MayWriteAnalyzer: public ModulePass {
		static char ID;
		MayWriteAnalyzer();
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual bool runOnModule(Module &M);

		/**
		 * Check if instruction <i> may write to <q>. 
		 * If <i> is a call instruction, the function may trace into the callee
		 * depending on flag <trace_callee>.
		 * Note that this function never traces into an exec-once function,
		 * becuase doing so is incorrect when used in <path_may_write> or
		 * <region_may_write>. 
		 */
		bool may_write(const Instruction *i, const Value *q,
				ConstFuncSet &visited_funcs, bool trace_callee = true);
		bool may_write(const Function *f, const Value *q,
				ConstFuncSet &visited_funcs, bool trace_callee = true);
		/**
		 * Check if an external call <cs> may write to <q>.
		 */
		bool libcall_may_write(const CallSite &cs, const Value *q);
	
	private:
		bool may_alias(const Value *v1, const Value *v2);
	};
}

#endif
