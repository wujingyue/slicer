/**
 * Author: Jingyue
 */

#include <fstream>
#include <sstream>
using namespace std;

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "common/InitializePasses.h"
#include "slicer/InitializePasses.h"
using namespace llvm;

#include "common/IDManager.h"
using namespace rcs;

#include "slicer/trace-manager.h"
using namespace slicer;

INITIALIZE_PASS_BEGIN(TraceManager, "trace-manager",
		"Trace manager", false, true)
INITIALIZE_PASS_DEPENDENCY(IDManager)
INITIALIZE_PASS_END(TraceManager, "trace-manager",
		"Trace manager", false, true)

void TraceManager::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<IDManager>();
}

static cl::opt<string> FullTraceFile("fulltrace",
		cl::desc("The full trace"));

char TraceManager::ID = 0;

bool TraceManager::read_record(istream &fin,
		TraceRecord &record) const {
	assert((fin.flags() | ios::binary) && "Must be a binary stream");
	if (fin.read((char *)&record, sizeof record))
		return true;
	else
		return false;
}

TraceManager::TraceManager(): ModulePass(ID), n_threads(0) {
	initializeTraceManagerPass(*PassRegistry::getPassRegistry());
}

bool TraceManager::runOnModule(Module &M) {
	records.clear();

	string full_trace_file = FullTraceFile;
	assert(full_trace_file != "" && "Didn't specify the full trace.");
	ifstream fin(full_trace_file.c_str(), ios::in | ios::binary);
	assert(fin && "Cannot open the full trace.");

	TraceRecord record;
	while (read_record(fin, record))
		records.push_back(record);

	compute_record_infos(M);

	validate_trace(M);

	return false;
}

void TraceManager::validate_trace(Module &M) {
}

void TraceManager::compute_record_infos(Module &M) {
	if (records.empty())
		return;
	raw_tid_to_tid.clear();
	n_threads = 0;
	// Map the raw main thread ID to 0.
	// Not necessary though, because the first record will be processed first.
	// But for safety reason, we put it here. 
	raw_tid_to_tid[records[0].raw_tid] = 0;
	++n_threads;
	for (size_t i = 0, E = records.size(); i < E; ++i) {
		IDManager &IDM = getAnalysis<IDManager>();
		TraceRecordInfo info;
		info.ins = IDM.getInstruction(records[i].ins_id);
		assert(info.ins);
		info.tid = get_normalized_tid(records[i].raw_tid);
		if (records[i].raw_child_tid == INVALID_RAW_TID) {
			info.child_tid = INVALID_TID;
		} else {
			raw_tid_to_tid[records[i].raw_child_tid] = n_threads;
			info.child_tid = n_threads;
			++n_threads;
		}
		record_infos.push_back(info);
	}
	assert(record_infos.size() == records.size());
}

int TraceManager::get_normalized_tid(unsigned long raw_tid) {
	assert(raw_tid != INVALID_RAW_TID);
	if (!raw_tid_to_tid.count(raw_tid)) {
		int new_thr_id = raw_tid_to_tid.size();
		raw_tid_to_tid[raw_tid] = new_thr_id;
	}
	return raw_tid_to_tid[raw_tid];
}

unsigned TraceManager::get_num_records() const {
	return records.size();
}

const TraceRecord &TraceManager::get_record(unsigned idx) const {
	assert(idx < records.size());
	return records[idx];
}

const TraceRecordInfo &TraceManager::get_record_info(unsigned idx) const {
	assert(idx < record_infos.size());
	return record_infos[idx];
}

void TraceManager::print(raw_ostream &O, const Module *M) const {
	for (size_t i = 0, E = record_infos.size(); i < E; ++i) {
		const TraceRecordInfo &info = record_infos[i];
		const TraceRecord &record = records[i];
		O << "[" << info.tid << "] " << record.ins_id;
		if (info.child_tid != INVALID_TID)
			O << " creates Thread " << info.child_tid;
		O << "\n";
	}
}
