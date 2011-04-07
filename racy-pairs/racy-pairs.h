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
		void print_context(const vector<User *> &cs) const;
		void extract_racy_pairs(
				int t1, unsigned s1, unsigned e1,
				int t2, unsigned s2, unsigned e2);
		size_t find_next_enforce(const vector<unsigned> &indices, size_t j) const;
		vector<User *> convert_context(const CallStack &cs) const;
		bool exclude_context(Instruction *ins, const vector<User *> &ctxt) const;
		/*
		 * Selects loads and stores invoked by <tid> from the index range [s, e). 
		 * Saves them to <load_stores>.
		 *
		 * This function filters out instructions
		 * invoked by white-listed functions.
		 * Specifically, it filters out an instruction when this instruction
		 * is in a white-listed function or any function on its callstack
		 * is white-listed. 
		 * 
		 * <selected_indices> returns the selected indices in the range [s, e). 
		 * If it's NULL, the function simply ignores it. 
		 * It's for debugging use only. 
		 */
		void select_load_store(
				unsigned s, unsigned e,
				int tid,
				vector<pair<Instruction *, vector<User *> > > &load_stores,
				vector<unsigned> *selected_indices);

		map<int, vector<DenseMap<unsigned, unsigned> > > clone_id_map;
		vector<InstPair> racy_pairs;
		vector<pair<unsigned, unsigned> > racy_pair_indices;
		int counter;
	};
}

#endif

