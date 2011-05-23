#include "landmark-trace.h"
#include "trace-manager.h"
#include "mark-landmarks.h"

namespace {

	static RegisterPass<slicer::LandmarkTrace> X(
			"slicer-landmark-trace",
			"Generates the landmark trace",
			false,
			true); // is analysis
}

namespace slicer {

	char LandmarkTrace::ID = 0;

	void LandmarkTrace::getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
		AU.addRequired<TraceManager>();
		AU.addRequired<MarkLandmarks>();
		ModulePass::getAnalysisUsage(AU);
	}

	vector<int> LandmarkTrace::get_thr_ids() const {
		map<int, vector<unsigned> >::const_iterator it;
		vector<int> res;
		for (it = thread_trunks.begin(); it != thread_trunks.end(); ++it)
			res.push_back(it->first);
		return res;
	}

	bool LandmarkTrace::runOnModule(Module &M) {

		TraceManager &TM = getAnalysis<TraceManager>();
		MarkLandmarks &ML = getAnalysis<MarkLandmarks>();

		for (unsigned i = 0, E = TM.get_num_records(); i < E; ++i) {
			const TraceRecordInfo &record_info = TM.get_record_info(i);
			if (ML.is_landmark(record_info.ins))
				thread_trunks[record_info.tid].push_back(i);
		}
		
		return false;
	}

	const vector<unsigned> &LandmarkTrace::get_thr_trunks(int thr_id) const {
		assert(thread_trunks.count(thr_id) &&
				"No records for the specified thread ID");
		return thread_trunks.find(thr_id)->second;
	}

	void LandmarkTrace::print(raw_ostream &O, const Module *M) const {
		vector<unsigned> all_indices;
		map<int, vector<unsigned> >::const_iterator it;
		for (it = thread_trunks.begin(); it != thread_trunks.end(); ++it) {
			all_indices.insert(
					all_indices.end(),
					it->second.begin(),
					it->second.end());
		}
		sort(all_indices.begin(), all_indices.end());
		for (size_t i = 0; i < all_indices.size(); ++i) {
			assert(i == 0 || all_indices[i - 1] < all_indices[i]);
			O << all_indices[i] << "\n";
		}
	}

	size_t LandmarkTrace::get_n_trunks(int thr_id) const {
		return get_thr_trunks(thr_id).size();
	}

	unsigned LandmarkTrace::get_landmark(int thr_id, size_t trunk_id) const {
		const vector<unsigned> &indices = get_thr_trunks(thr_id);
		assert(trunk_id < indices.size());
		return indices[trunk_id];
	}
}
