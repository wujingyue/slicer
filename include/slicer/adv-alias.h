/**
 * Author: Jingyue
 */

#ifndef __SLICER_ADV_ALIAS_H
#define __SLICER_ADV_ALIAS_H

#include "llvm/Pass.h"
#include "llvm/Analysis/AliasAnalysis.h"
using namespace llvm;

#include "common/typedefs.h"
using namespace rcs;

namespace slicer {
	struct QueryInfo {
		QueryInfo(bool s, const Value *a, const Value *b, bool r):
			satisfiable(s), v1(a), v2(b), result(r) {}

		bool satisfiable;
		const Value *v1, *v2;
		bool result;
	};

	struct AdvancedAlias: public ModulePass, public AliasAnalysis {
		static char ID;

		AdvancedAlias();
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
		virtual void *getAdjustedAnalysisPointer(AnalysisID PI) {
			if (PI == &AliasAnalysis::ID)
				return (AliasAnalysis*)this;
			return this;
		}
		// For debugging purpose. 
		size_t get_cache_size() const {
			return may_cache.size() + must_cache.size();
		}
		/**
		 * AliasAnalysis interfaces.
		 * Returns: NoAlias, MayAlias, or MustAlias
		 */
		virtual AliasResult alias(const Location &L1, const Location &L2);
		AliasResult alias(
				const ConstInstList &c1, const Value *v1,
				const ConstInstList &c2, const Value *v2);
		/**
		 * May aliasing seems pretty slow, but must aliasing is fast. 
		 * Therefore, we provide this interface to perform fast must-aliasing
		 * queries. 
		 */
		bool must_alias(const Value *v1, const Value *v2);
		bool must_alias(const Use *u1, const Use *u2);
		/**
		 * Separate interface for may-aliasing queries. 
		 */
		bool may_alias(const Value *v1, const Value *v2);
		bool may_alias(const Use *u1, const Use *u2);

		void get_must_alias_pairs(vector<ConstValuePair> &must_alias_pairs) const;

	private:
		void print_average_query_time(raw_ostream &O) const;

		bool check_may_cache(const Value *v1, const Value *v2, bool &res);
		bool check_must_cache(const Value *v1, const Value *v2, bool &res);
		void add_to_may_cache(const Value *v1, const Value *v2, bool res);
		void add_to_must_cache(const Value *v1, const Value *v2, bool res);

		static ConstValuePair make_ordered_value_pair(
				const Value *v1, const Value *v2);

		DenseMap<ConstValuePair, bool> may_cache; // Cache satisfiable() results. 
		DenseMap<ConstValuePair, bool> must_cache; // Cache provable() results.
		vector<pair<clock_t, QueryInfo> > query_times;
	};
}

#endif
