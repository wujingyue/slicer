/**
 * Author: Jingyue
 *
 * More aggressive LoadInst promotion. 
 */

#ifndef __SLICER_AGGRESIVE_PROMOTION_H
#define __SLICER_AGGRESIVE_PROMOTION_H

#include "llvm/Analysis/LoopPass.h"
#include "common/typedefs.h"
using namespace llvm;

namespace slicer {

	struct AggressivePromotion: public LoopPass {

		static char ID;

		AggressivePromotion(): LoopPass(ID) {}
		// The outmost loop will be processed last. 
		virtual bool runOnLoop(Loop *L, LPPassManager &LPM);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
	
	private:
		// TODO: Refactor. Similar code used in Capturer as well. 
		bool path_may_write(
				const Instruction *i1, const Instruction *i2, const Value *p);
		bool may_write(const Loop *L, const Value *p);
		bool may_write(
				const Instruction *i, const Value *q, ConstFuncSet &visited_funcs);
		bool may_write(
				const Function *f, const Value *q, ConstFuncSet &visited_funcs);
		// <li> must be directly contained in loop <L>.
		// i.e. not in any subloop of <L>.
		bool should_promote(LoadInst *li, Loop *L);
	};
}

#endif
