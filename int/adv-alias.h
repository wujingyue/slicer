/**
 * Author: Jingyue
 */

#ifndef __SLICER_ADV_ALIAS_H
#define __SLICER_ADV_ALIAS_H

#include "llvm/Pass.h"
#include "llvm/Analysis/AliasAnalysis.h"
using namespace llvm;

namespace slicer {

	struct QueryInfo {

		QueryInfo(bool s, const Value *a, const Value *b):
			satisfiable(s), v1(a), v2(b) {}

		bool satisfiable;
		const Value *v1, *v2;
	};

	struct AdvancedAlias: public ModulePass, public AliasAnalysis {

		static char ID;

		AdvancedAlias(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		void recalculate(Module &M);
		virtual void print(raw_ostream &O, const Module *M) const;
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void releaseMemory();
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
		size_t get_cache_size() const { return may_cache.size(); }
		/*
		 * AliasAnalysis interfaces.
		 * Returns: NoAlias, MayAlias, or MustAlias
		 */
		virtual AliasResult alias(
				const Value *V1, unsigned V1Size,
				const Value *V2, unsigned V2Size);
		/*
		 * May aliasing seems pretty slow, but must aliasing is fast. 
		 * Therefore, we provide this interface to perform fast must-aliasing
		 * queries. 
		 */
		bool must_alias(const Value *V1, const Value *V2);

	private:
		void print_average_query_time(raw_ostream &O) const;

		DenseMap<ConstValuePair, bool> may_cache; // Cache satisfiable() results. 
		DenseMap<ConstValuePair, bool> must_cache; // Cache provable() results.
		vector<pair<clock_t, QueryInfo> > query_times;
	};
}

#endif
