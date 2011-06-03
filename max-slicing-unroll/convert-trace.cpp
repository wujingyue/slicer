#include "llvm/Support/CommandLine.h"
#include "idm/id.h"
using namespace llvm;

#include <fstream>
#include <sstream>
using namespace std;

#include "convert-trace.h"
#include "trace/trace-manager.h"
#include "trace/landmark-trace.h"
#include "max-slicing-unroll/clone-map-manager.h"

/* Apply it on the cloned program. */
namespace {
	
	static RegisterPass<slicer::ConvertTrace> X(
			"convert-trace",
			"Convert a full trace for the original program to the full "
			"trace for the cloned program",
			false,
			true); // is analysis
	static cl::opt<string> OutputFile(
			"output",
			cl::ValueRequired,
			cl::desc("The output full trace"),
			cl::init(""));
}

namespace slicer {

	bool ConvertTrace::runOnModule(Module &M) {

		string output_file = OutputFile;
		assert(output_file != "" && "Didn't specify the output file");
		ofstream fout(output_file.c_str(), ios::binary);
		assert(fout && "Cannot open the specified full trace");

		TraceManager &TM = getAnalysis<TraceManager>();
		LandmarkTrace &LT = getAnalysis<LandmarkTrace>();
		for (unsigned i = 0; i < TM.get_num_records(); ++i) {
			// Will be modified.
			TraceRecord record = TM.get_record(i);
			TraceRecordInfo info = TM.get_record_info(i);
			unsigned ins_id = record.ins_id;
			assert(ins_id != ObjectID::INVALID_ID);
			int tid = info.tid;
			// Figure out the trunk ID. 
			// TODO: LandmarkTrace should provide this interface. 
			const vector<unsigned> &thr_trunks = LT.get_thr_trunks(tid);
			size_t tr_id = (upper_bound(
						thr_trunks.begin(),
						thr_trunks.end(),
						i) - thr_trunks.begin()) - 1;
			if (tr_id == (size_t)(-1)) {
				// Even before the first trunk. 
				// Possible because the first executed instruction in a
				// thread may not be a landmark. 
				// In this case, use the original instruction. 
				tid = -1;
				tr_id = 0;
			}
			// Find the cloned instruction. 
			CloneMapManager &CMM = getAnalysis<CloneMapManager>();
			Instruction *new_ins = CMM.get_cloned_inst(tid, tr_id, info.ins);
			ObjectID &OI = getAnalysis<ObjectID>();
			unsigned new_ins_id = OI.getInstructionID(new_ins);
			// Shouldn't use tid because it might be changed to -1 
			// in order to look up the clone mapping. 
			TraceRecord new_record;
			new_record.ins_id = new_ins_id;
			new_record.raw_tid = info.tid;
			new_record.raw_child_tid = info.child_tid;
		}

		return false;
	}

	void ConvertTrace::getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
		AU.addRequired<ObjectID>();
		AU.addRequired<LandmarkTrace>();
		AU.addRequired<TraceManager>();
		AU.addRequired<CloneMapManager>();
		ModulePass::getAnalysisUsage(AU);
	}

	char ConvertTrace::ID = 0;
}

