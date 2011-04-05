#ifndef __SLICER_RACY_PAIRS_H
#define __SLICER_RACY_PARIS_H

#include "llvm/Module.h"

#include "common/include/typedefs.h"
#include "llvm-instrument/add-calling-context/add-calling-context.h"

#include <map>
#include <vector>

using namespace llvm;
using namespace std;
using namespace tern;

namespace slicer {

	struct RacyPairs: public ModulePass {

		static char ID;

		typedef map<int, vector<unsigned> > ThreadToTrunk;

		RacyPairs(): ModulePass(&ID), counter(0) {}
		
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;

	private:
		void read_trunks(const string &trace_file);
		void read_clone_map(const string &clone_map_file);
		void print_inst(Instruction *ins, raw_ostream &O) const;
		void dump_inst(Instruction *ins) const;
		void print_context(const CallStack &cs) const;
		void extract_racy_pairs(
				int t1, unsigned s1, unsigned e1,
				int t2, unsigned s2, unsigned e2);
		size_t find_next_enforce(const vector<unsigned> &indices, size_t j) const;
		vector<User *> compute_context(
				const CallStack &cs,
				int thr_id,
				bool cloned) const;
		/*
		 * Compute the instruction according to its trace index and tid. 
		 * If <cloned> is specified, returns the cloned instruction. 
		 */
		Instruction *compute_inst(
				unsigned idx,
				int thr_id,
				bool cloned) const;
		/*
		 * Selects loads and stores invoked by <tid> from the index range [s, e). 
		 * Saves them to <load_stores>.
		 * If <cloned> is true, we look at their counterparts
		 * in the cloned program. 
		 */
		void select_load_store(
				unsigned s, unsigned e,
				int tid,
				bool cloned,
				vector<pair<Instruction *, CallStack> > &load_stores);

		ThreadToTrunk trunks;
		map<int, vector<InstMapping> > clone_map;
		vector<InstPair> racy_pairs;
		int counter;
	};
}

#endif

