#ifndef __SLICER_TRACE_MANAGER_H
#define __SLICER_TRACE_MANAGER_H

#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/ADT/DenseMap.h"
using namespace llvm;

#include <map>
#include <vector>
using namespace std;

#include "trace.h"

namespace slicer {
	
	enum TraceRecordType {
		TR_DEFAULT = 0,
		TR_LANDMARK_ENFORCE = 1,
		TR_LANDMARK_NO_ENFORCE = 2 // not used for now
	};

	// Computed by TraceManager
	struct TraceRecordInfo {
		Instruction *ins;
		TraceRecordType type;
		// Normalized thread ID. Starts from 0. The main thread ID is always 0
		int tid;
		int child_tid; // Normalized child thread ID
	};

	struct TraceManager: public ModulePass {

		static char ID;

		const static unsigned INVALID_IDX = (unsigned)(-1);

		TraceManager(): ModulePass(&ID) {}

		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		const TraceRecord &get_record(unsigned idx) const;
		const TraceRecordInfo &get_record_info(unsigned idx) const;
		unsigned get_num_records() const;
		// Used by the trace converter. 
		bool write_record(ostream &fout, const TraceRecord &record) const;

	private:
		/*
		 * Returns true if we successfully get a trace record. 
		 */
		bool read_record(istream &fin, TraceRecord &record) const;
		/*
		 * Returns false if the line does not specify a record.
		 */
		bool gen_record(const TraceRecord &record, string &str) const;
		int get_normalized_tid(unsigned long raw_tid);
		void compute_record_infos(Module &M);

		vector<TraceRecord> records;
		vector<TraceRecordInfo> record_infos;
		DenseMap<unsigned long, int> raw_tid_to_tid;
	};
}

#endif
