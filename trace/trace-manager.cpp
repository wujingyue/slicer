#include "llvm/Support/CommandLine.h"
#include "idm/id.h"
using namespace llvm;

#include <fstream>
#include <sstream>
using namespace std;

#include <boost/regex.hpp>
using namespace boost;

#include "trace-manager.h"
#include "slicer-landmarks.h"

namespace {
	
	static cl::opt<string> FullTraceFile(
			"fulltrace",
			cl::ValueRequired,
			cl::desc("The full trace"));
}

namespace slicer {

	bool TraceManager::read_record(
			istream &fin,
			TraceRecord &record) const {
		assert((fin.flags() | ios::binary) && "Must be a binary stream");
		if (fin.read((char *)&record, sizeof record))
			return true;
		else
			return false;
	}

	bool TraceManager::runOnModule(Module &M) {
		string full_trace_file = FullTraceFile;
		ifstream fin(full_trace_file.c_str(), ios::binary);
		assert(fin && "Cannot open the specified full trace");

		records.clear();
		TraceRecord record;
		while (read_record(fin, record))
			records.push_back(record);

		compute_record_infos(M);
		return false;
	}

	void TraceManager::compute_record_infos(Module &M) {
		if (records.empty())
			return;
		raw_tid_to_tid.clear();
		// Map the raw main thread ID to 0.
		// Not necessary though, because the first record will be processed first.
		// But for safety reason, we put it here. 
		raw_tid_to_tid[records[0].raw_tid] = 0;
		for (size_t i = 0, E = records.size(); i < E; ++i) {
			ObjectID &OI = getAnalysis<ObjectID>();
			TraceRecordInfo info;
			info.ins = OI.getInstruction(records[i].ins_id);
			assert(info.ins && "Cannot find the instruction");
			if (is_app_landmark(info.ins))
				info.type = TR_LANDMARK_ENFORCE;
			else
				info.type = TR_DEFAULT;
			info.tid = get_normalized_tid(records[i].raw_tid);
			info.child_tid = get_normalized_tid(records[i].raw_child_tid);
			record_infos.push_back(info);
		}
	}

	int TraceManager::get_normalized_tid(unsigned long raw_tid) {
		if (raw_tid == INVALID_RAW_TID)
			return -1;
		if (!raw_tid_to_tid.count(raw_tid))
			raw_tid_to_tid[raw_tid] = (int)raw_tid_to_tid.size();
		return raw_tid_to_tid[raw_tid];
	}

	void TraceManager::getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
		AU.addRequired<ObjectID>();
		ModulePass::getAnalysisUsage(AU);
	}

	unsigned TraceManager::get_num_records() const {
		return records.size();
	}

	const TraceRecord &TraceManager::get_record(unsigned idx) const {
		assert(idx < records.size());
		return records[idx];
	}

	char TraceManager::ID = 0;
}

