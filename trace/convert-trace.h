#ifndef __TERN_CONVERT_TRACE_H
#define __TERN_CONVERT_TRACE_H

#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/ADT/DenseMap.h"
using namespace llvm;

#include <map>
#include <vector>
using namespace std;

#include "trace-manager.h"
#include "trace.h"

namespace slicer {
	
	struct ConvertTrace: public ModulePass {

		static char ID;

		const static unsigned INVALID_IDX = (unsigned)(-1);

		ConvertTrace(): ModulePass(&ID) {}

		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;

	private:
		void read_clone_map(const string &clone_map_file);
		bool write_record(
				ostream &fout,
				unsigned idx,
				unsigned ins_id,
				int thr_id,
				int child_tid,
				TraceRecordType type) const;

		map<int, vector<DenseMap<unsigned, unsigned> > > clone_map;
	};
}

#endif

