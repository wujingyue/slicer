#ifndef __SLICER_RACY_PAIRS_H
#define __SLICER_RACY_PARIS_H

#include "llvm/Module.h"

#include "common/include/typedefs.h"
#include "llvm-instrument/trace/add-calling-context.h"

#include <map>
#include <vector>

using namespace tern;
using namespace llvm;
using namespace std;

namespace slicer {

	struct RacyPairs: public ModulePass {

		static char ID;

		RacyPairs(): ModulePass(&ID), counter(0) {}
		
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;

	private:
		void print_inst(Instruction *ins, raw_ostream &O) const;
		void dump_inst(Instruction *ins) const;
		void print_context(const CallStack &cs) const;
		void extract_racy_pairs(
				int t1, unsigned s1, unsigned e1,
				int t2, unsigned s2, unsigned e2);
		size_t find_next_enforce(const vector<unsigned> &indices, size_t j) const;
		vector<User *> convert_context(const CallStack &cs) const;
		/*
		 * Selects loads and stores invoked by <tid> from the index range [s, e). 
		 * Saves them to <load_stores>.
		 * If <CloneMapFile> is specified, we look at their counterparts
		 * in the cloned program. 
		 */
		void select_load_store(
				unsigned s, unsigned e,
				int tid,
				vector<pair<Instruction *, CallStack> > &load_stores);

		map<int, vector<DenseMap<unsigned, unsigned> > > clone_id_map;
		vector<InstPair> racy_pairs;
		int counter;
	};
}

#endif

