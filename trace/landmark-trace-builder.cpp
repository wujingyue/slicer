#include "llvm/Support/CommandLine.h"
using namespace llvm;

#include "landmark-trace-record.h"
#include "landmark-trace-builder.h"
#include "trace-manager.h"
#include "mark-landmarks.h"
#include "slicer-landmarks.h"
using namespace slicer;

#include <fstream>
using namespace std;

static RegisterPass<LandmarkTraceBuilder> X(
		"build-landmark-trace",
		"Generates the landmark trace",
		false,
		true); // is analysis

static cl::opt<string> LandmarkTraceFile(
		"output-landmark-trace",
		cl::desc("The output landmark trace"),
		cl::init(""));

char LandmarkTraceBuilder::ID = 0;

void LandmarkTraceBuilder::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<TraceManager>();
	AU.addRequired<MarkLandmarks>();
	ModulePass::getAnalysisUsage(AU);
}

bool LandmarkTraceBuilder::runOnModule(Module &M) {

	TraceManager &TM = getAnalysis<TraceManager>();
	MarkLandmarks &ML = getAnalysis<MarkLandmarks>();

	assert(LandmarkTraceFile != "" && "Didn't specify the output file");
	ofstream fout(LandmarkTraceFile.c_str(), ios::out | ios::binary);
	assert(fout && "Cannot open the output file");
	for (unsigned i = 0, E = TM.get_num_records(); i < E; ++i) {
		const TraceRecord &record = TM.get_record(i);
		const TraceRecordInfo &record_info = TM.get_record_info(i);
		if (ML.is_landmark(record_info.ins)) {
			LandmarkTraceRecord lt_record;
			lt_record.idx = i;
			lt_record.ins_id = record.ins_id;
			lt_record.enforcing = is_app_landmark(record_info.ins);
			lt_record.tid = record_info.tid;
			lt_record.child_tid = record_info.child_tid;
			fout.write((char *)&lt_record, sizeof lt_record);
		}
	}

	return false;
}
