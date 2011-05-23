#include "llvm/Support/CommandLine.h"
#include "idm/id.h"
using namespace llvm;

#include <fstream>
#include <sstream>
using namespace std;

#include <boost/regex.hpp>
using namespace boost;

#include "convert-trace.h"
#include "trace-manager.h"
#include "landmark-trace.h"

namespace {
	
	static RegisterPass<slicer::ConvertTrace> X(
			"tern-convert-trace",
			"Convert a full trace for the original program to the full "
			"trace for the cloned program",
			false,
			true); // is analysis
	static cl::opt<string> OutputFile(
			"output",
			cl::ValueRequired,
			cl::desc("The output full trace"),
			cl::init(""));
	static cl::opt<string> CloneMapFile(
			"clone-map",
			cl::NotHidden,
			cl::ValueRequired,
			cl::desc("The clone mapping"));
}

namespace slicer {

	bool ConvertTrace::write_record(
			ostream &fout,
			unsigned idx,
			unsigned ins_id,
			int thr_id,
			int child_tid,
			TraceRecordType type) const {

		assert(ins_id != ObjectID::INVALID_ID);

		fout << "TRACE: IDX: " << idx << ": "
			<< "IID: " << ins_id << ": "
			<< "TID: " << thr_id << ": "
			<< "CHILDTID: " << child_tid << ": ";
		if (type == TR_DEFAULT) {
			// Do nothing
		} else if (type == TR_LANDMARK_ENFORCE) {
			fout << "CALLED: tern_wrap_";
		} else if (type == TR_LANDMARK_NO_ENFORCE) {
			assert(false && "Not implemented");
		} else {
			assert(false && "Invalid record type");
		}
		fout << endl;
		return true;
	}

	void ConvertTrace::read_clone_map(const string &clone_map_file) {
		assert(clone_map_file.length() > 0 && "Didn't specify -clone-map");
		ifstream fin(clone_map_file.c_str());
		assert(fin && "Cannot open the specified clone map file");

		clone_map.clear();
		string line;
		while (getline(fin, line)) {
			istringstream iss(line);
			int thr_id;
			size_t trunk_id;
			unsigned orig_id, cloned_id;
			if (iss >> thr_id >> trunk_id >> orig_id >> cloned_id) {
				while (trunk_id >= clone_map[thr_id].size())
					clone_map[thr_id].push_back(DenseMap<unsigned, unsigned>());
				clone_map[thr_id][trunk_id][orig_id] = cloned_id;
			}
		}
	}

	bool ConvertTrace::runOnModule(Module &M) {

		read_clone_map(CloneMapFile);

		string output_file = OutputFile;
		assert(output_file != "" && "Didn't specify the output file");
		ofstream fout(output_file.c_str());
		assert(fout && "Cannot open the specified full trace");

		TraceManager &TM = getAnalysis<TraceManager>();
		LandmarkTrace &LT = getAnalysis<LandmarkTrace>();
		for (unsigned i = 0; i < TM.get_num_records(); ++i) {
			// Will be modified.
			TraceRecord record = TM.get_record(i);
			TraceRecordInfo record_info = TM.get_record_info(i);
			unsigned ins_id = record.ins_id;
			assert(ins_id != ObjectID::INVALID_ID);
			int tid = record_info.tid;
			// Figure out the trunk ID. 
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
			const vector<DenseMap<unsigned, unsigned> > &thr_clone_map =
				clone_map.find(tid)->second;
			assert(tr_id < thr_clone_map.size());
			unsigned new_ins_id;
			if (thr_clone_map[tr_id].count(ins_id))
				new_ins_id = thr_clone_map[tr_id].lookup(ins_id);
			else {
				assert(clone_map[-1][0].count(ins_id));
				new_ins_id = clone_map[-1][0].lookup(ins_id);
			}
			// Shouldn't use tid because it might be changed to -1 
			// in order to look up the clone mapping. 
			write_record(fout, i, new_ins_id, record_info.tid,
					record_info.child_tid, record_info.type);
		}

		return false;
	}

	void ConvertTrace::getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
		AU.addRequired<ObjectID>();
		AU.addRequired<LandmarkTrace>();
		AU.addRequired<TraceManager>();
		ModulePass::getAnalysisUsage(AU);
	}

	char ConvertTrace::ID = 0;
}

