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

		RacyPairs(): ModulePass(&ID) {}
		
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;

	private:
		void read_trunks(
				const string &trace_file,
				ThreadToTrunk &trunks) const;
		void read_clone_map(
				const string &clone_map_file,
				map<int, vector<InstMapping> > &clone_map) const;
		void print_inst(Instruction *ins, raw_ostream &O) const;
		void extract_racy_pairs(
				int t1, unsigned s1, unsigned e1,
				int t2, unsigned s2, unsigned e2,
				const ThreadToTrunk &trunks,
				const map<int, vector<InstMapping> > &clone_map);
		size_t find_next_enforce(const vector<unsigned> &indices, size_t j) const;
		static vector<CallInst *> convert_context(const CallStack &cs);

		vector<InstPair> racy_pairs;
	};
}

#endif

