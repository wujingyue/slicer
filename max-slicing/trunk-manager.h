#ifndef __SLICER_TRUNK_MANAGER_H
#define __SLICER_TRUNK_MANAGER_H

#include "llvm/Pass.h"
#include "llvm/Instruction.h"
#include "llvm/ADT/DenseSet.h"
#include "common/include/typedefs.h"
using namespace llvm;

#include <vector>
#include <climits>
using namespace std;

#if 0
namespace llvm {
	
	template<> struct DenseMapInfo<int> {
		static inline int getEmptyKey() { return INT_MAX; }
		static inline int getTombstoneKey() { return INT_MAX - 1; }
		static int getHashValue(const int &val) { return val * 37; }
		static bool isEqual(const int &a, const int &b) { return a == b; }
	};
}
#endif

namespace slicer {

	struct TrunkManager: public ModulePass {

		static char ID;

		TrunkManager(): ModulePass(&ID) {}
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual bool runOnModule(Module &M);
		
		/**
		 * Given an instruction, outputs a list of trunks in which
		 * this instruction may be executed in the sliced program. 
		 */
		void get_containing_trunks(
				const Instruction *ins,
				vector<pair<int, size_t> > &containing_trunks) const;
		/**
		 * Returns all trunks that are concurrent with the given trunk. 
		 */
		void get_concurrent_trunks(
				const pair<int, size_t> &the_trunk,
				vector<pair<int, size_t> > &concurrent_trunks) const;

	private:
		/**
		 * Used in <get_concurrent_trunk>. 
		 * Recall that there are two types of landmarks:
		 * enforcing landmarks and derived landmarks. 
		 * The order of the derived landmarks are not enforced. 
		 * i.e. Even if Trunk i happens before Trunk j in the log, Trunk i may
		 * not always happen before Trunk j if Trunk i does not end with an
		 * enforcing landmark or Trunk j does not start with an enforcing landmark.
		 * This function tries to extend the trunk region [s, e] forward and
		 * backward until the region is bounded by enforcing landmarks. 
		 * 
		 * When calling this function, <s> and <e> indicate the original region. 
		 * After it returns, <s> and <e> will indicate the extended region. 
		 */
		void extend_until_enforce(int thr_id, size_t &s, size_t &e) const;
		void search_containing_trunks(
				const Instruction *ins,
				ConstInstSet &visited,
				vector<pair<int, size_t> > &containing_trunks) const;
	};
}

#endif
