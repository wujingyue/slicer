/**
 * Author: Jingyue
 */

#ifndef __SLICER_ADV_ALIAS_H
#define __SLICER_ADV_ALIAS_H

#include "llvm/Pass.h"
#include "llvm/Analysis/AliasAnalysis.h"
using namespace llvm;

namespace slicer {

	struct AdvancedAlias: public ModulePass, public AliasAnalysis {

		static char ID;

		AdvancedAlias(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		bool recalculate(Module &M);
		virtual void print(raw_ostream &O, const Module *M) const;
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		/** 
		 * This method is used when a pass implements
		 * an analysis interface through multiple inheritance.  If needed, it
		 * should override this to adjust the this pointer as needed for the
		 * specified pass info.
		 */
		virtual void *getAdjustedAnalysisPointer(const PassInfo *PI) {   
			if (PI->isPassID(&AliasAnalysis::ID))
				return (AliasAnalysis*)this;
			return this;
		}
		// For debugging purpose. 
		size_t get_cache_size() const {
			return cache.size();
		}
		/*
		 * AliasAnalysis interfaces.
		 * Returns: NoAlias, MayAlias, or MustAlias
		 */
		virtual AliasResult alias(
				const Value *V1, unsigned V1Size,
				const Value *V2, unsigned V2Size);

	private:
		DenseMap<ConstValuePair, AliasResult> cache;
	};
}

#endif
