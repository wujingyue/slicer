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
		DynamicInstruction(int thr_id, size_t tr_id, const Instruction *i):
			thread_id(thr_id), trunk_id(tr_id), ins(i) {}
		int thread_id;
		size_t trunk_id;
		const Instruction *ins;
	};
	bool operator==(const DynamicInstruction &a, const DynamicInstruction &b) {
		return a.thread_id == b.thread_id &&
			a.trunk_id == b.trunk_id &&
			a.ins == b.ins;
	}
	
	struct DynamicInstructionWithContext {
		DynamicInstructionWithContext(int thr_id, size_t tr_id,
				const Instruction *i, const vector<DynamicInstruction> &cs):
			di(thr_id, tr_id, i), callstack(cs) {}
		DynamicInstructionWithContext(int thr_id, size_t tr_id,
				const Instruction *i): di(thr_id, tr_id, i) {}
		DynamicInstruction di;
		vector<DynamicInstruction> callstack;
	};
}
using namespace slicer;

namespace llvm {
	template<> struct DenseMapInfo<DynamicInstruction> {
		static inline DynamicInstruction getEmptyKey() {
			return DynamicInstruction(0, 0, NULL);
		}
		static inline DynamicInstruction getTombstoneKey() {
			return DynamicInstruction(-1, (size_t)-1, NULL);
		}
		static unsigned getHashValue(const DynamicInstruction &DI) {
			return DenseMapInfo<pair<const Instruction *, unsigned> >::getHashValue(
					make_pair(DI.ins,
						DenseMapInfo<pair<int, size_t> >::getHashValue(
							make_pair(DI.thread_id, DI.trunk_id))));
		}
		static bool isEqual(const DynamicInstruction &a,
				const DynamicInstruction &b) {
			return a == b;
		}
	};

	template<> struct DenseMapInfo<DynamicInstructionWithContext> {
		static inline DynamicInstructionWithContext getEmptyKey() {
			return DynamicInstructionWithContext(0, 0, NULL,
					vector<DynamicInstruction>());
		}
		static inline DynamicInstructionWithContext getTombstoneKey() {
			return DynamicInstructionWithContext(-1, (size_t)-1, NULL,
					vector<DynamicInstruction>());
		}
		static unsigned getHashValue(const DynamicInstructionWithContext &DIWC) {
			return DenseMapInfo<DynamicInstruction>::getHashValue(DIWC.di);
		}
		static bool isEqual(const DynamicInstructionWithContext &a,
				const DynamicInstructionWithContext &b) {
			return a.di == b.di;
		}
	};
}

namespace slicer {
	struct QueryGenerator: public ModulePass {
		static char ID;

		QueryGenerator();
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;

	private:
		void print_dynamic_instruction(raw_ostream &O,
				const DynamicInstruction &di) const;
		void print_dynamic_instruction_with_context(raw_ostream &O,
				const DynamicInstructionWithContext &diwc) const;
		void generate_static_queries(Module &M);
		void generate_dynamic_queries(Module &M);

		vector<pair<DynamicInstructionWithContext,
			DynamicInstructionWithContext> > all_queries;
	};
}

#endif
