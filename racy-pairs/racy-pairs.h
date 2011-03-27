#ifndef __SLICER_RACY_PAIRS_H
#define __SLICER_RACY_PARIS_H

#include "llvm/Module.h"

#include "common/include/typedefs.h"

#include <map>
#include <vector>

using namespace llvm;
using namespace std;

namespace slicer {

	struct RacyPairs: public ModulePass {

		static char ID;

		/*
		 * Keys are thread IDs. 
		 * The value of a key is a list of starting positions of trunks. 
		 */
		typedef map<int, vector<int> > Trace;

		RacyPairs(): ModulePass(&ID) {}
		
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;

	private:
		void read_trace(const string &trace_file, Trace &trace) const;
		void read_clone_map(
				const string &clone_map_file,
				map<int, vector<InstMapping> > &clone_map) const;
		void print_inst(Instruction *ins, raw_ostream &O) const;
		void extract_racy_pairs(const InstMapping &m1, const InstMapping &m2);

		vector<InstPair> racy_pairs;
	};
}

#endif

