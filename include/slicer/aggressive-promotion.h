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

		AggressivePromotion();
		// The outmost loop will be processed last. 
		virtual bool runOnLoop(Loop *L, LPPassManager &LPM);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
	
	private:
		bool hoist_region(Loop *L, DomTreeNode *node);
		bool can_hoist(Loop *L, Instruction *ins);
		bool is_safe_to_execute_unconditionally(Loop *L, Instruction *ins);
		bool loop_may_write(const Loop *L, const Value *p);

		DominatorTree *DT;
		LoopInfo *LI;
	};
}

#endif
