#ifndef __SLICER_QUERY_GEN_H
#define __SLICER_QUERY_GEN_H

#include <vector>
using namespace std;

#include "llvm/Instructions.h"
#include "llvm/Pass.h"
#include "common/typedefs.h"
using namespace llvm;

namespace slicer {
	struct DynamicInstruction {
		DynamicInstruction(const Instruction *i, const ConstInstList &cs,
				int thr_id, size_t tr_id):
			ins(i), callstack(cs), thread_id(thr_id), trunk_id(tr_id) {}
		const Instruction *ins;
		ConstInstList callstack;
		int thread_id;
		size_t trunk_id; // The thread ID is included in Region.
	};
}
using namespace slicer;

namespace llvm {
	template<> struct DenseMapInfo<DynamicInstruction> {
		static inline DynamicInstruction getEmptyKey() {
			return DynamicInstruction(NULL, ConstInstList(), 0, 0);
		}
		static inline DynamicInstruction getTombstoneKey() {
			return DynamicInstruction(NULL, ConstInstList(), -1, (size_t)-1);
		}
		static unsigned getHashValue(const DynamicInstruction &DI) {
			return DenseMapInfo<pair<const Instruction *, unsigned> >::getHashValue(
					make_pair(DI.ins,
						DenseMapInfo<pair<int, size_t> >::getHashValue(
							make_pair(DI.thread_id, DI.trunk_id))));
		}
		static bool isEqual(const DynamicInstruction &a,
				const DynamicInstruction &b) {
			return a.ins == b.ins && a.thread_id == b.thread_id &&
				a.trunk_id == b.trunk_id;
		}
	};
}

namespace slicer {
	struct QueryGenerator: public ModulePass {
		static char ID;

		QueryGenerator(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;

	private:
		void print_dynamic_instruction(raw_ostream &O,
				const DynamicInstruction &di) const;
		void generate_static_queries(Module &M);
		void generate_dynamic_queries(Module &M);

		vector<pair<DynamicInstruction, DynamicInstruction> > all_queries;
	};
}

#endif
