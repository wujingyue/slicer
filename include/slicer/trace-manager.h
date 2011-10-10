/**
 * Author: Jingyue
 *
 * TODO: Make the TraceManager an AnalysisGroup, so that we can plugin
 * customized TraceManager's. 
 *
 * Note that TraceManager may be used to read the old trace for a sliced
 * program. Therefore, not all instructions and instruction IDs can still
 * be found in the module. 
 */

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
using namespace slicer;

namespace slicer {
	struct TraceRecordInfo {
		Instruction *ins;
		// Normalized thread ID. Starts from 0. The main thread ID is always 0
		int tid;
		int child_tid; // Normalized child thread ID
	};

	struct TraceManager: public ModulePass {
		static char ID;

		const static unsigned INVALID_IDX = (unsigned)(-1);
		const static int INVALID_TID = -1;

		TraceManager(): ModulePass(&ID), n_threads(0) {}

		virtual bool runOnModule(Module &M);
		virtual void print(raw_ostream &O, const Module *M) const;
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		const TraceRecord &get_record(unsigned idx) const;
		const TraceRecordInfo &get_record_info(unsigned idx) const;
		unsigned get_num_records() const;
		// Used by the trace converter. 
		bool write_record(ostream &fout, const TraceRecord &record) const;

	private:
		/**
		 * Returns true if we successfully get a trace record. 
		 * fin must be opened in the binary mode.
		 */
		bool read_record(istream &fin, TraceRecord &record) const;
		int get_normalized_tid(unsigned long raw_tid);
		void compute_record_infos(Module &M);
		void validate_trace(Module &M);

		vector<TraceRecord> records;
		vector<TraceRecordInfo> record_infos;
		DenseMap<unsigned long, int> raw_tid_to_tid;
		/**
		 * # of used threads. 
		 * May not equal raw_tid_to_tid.size() because pthread_create reuses
		 * thread IDs. 
		 */
		unsigned n_threads;
	};
}

#endif
