#include "landmark-trace.h"
#include "trace-manager.h"
#include "mark-landmarks.h"
using namespace slicer;

static RegisterPass<LandmarkTrace> X(
		"landmark-trace",
		"Generates the landmark trace",
		false,
		true); // is analysis

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

void LandmarkTrace::get_concurrent_trunks(
		const pair<int, size_t> &the_trunk,
		vector<pair<int, size_t> > &concurrent_trunks) const {
	size_t s = the_trunk.second, e = the_trunk.second;
	extend_until_enforce(the_trunk.first, s, e);
	unsigned s_idx = get_landmark(the_trunk.first, s);
	unsigned e_idx = get_landmark(the_trunk.first,
			(e + 1 == get_n_trunks(the_trunk.first) ? e : e + 1));
	vector<int> thr_ids = get_thr_ids();
	for (size_t i = 0; i < thr_ids.size(); ++i) {
		int thr_id = thr_ids[i];
		if (thr_id == the_trunk.first)
			continue;
		const vector<unsigned> &thr_trunks = get_thr_trunks(thr_id);
		vector<unsigned>::const_iterator it;
		it = lower_bound(thr_trunks.begin(), thr_trunks.end(), s_idx);
		if (it != thr_trunks.begin())
			--it;
		size_t s1 = it - thr_trunks.begin();
		it = lower_bound(thr_trunks.begin(), thr_trunks.end(), e_idx);
		if (it == thr_trunks.begin()) {
			// The first landmark in Thread <thr_id> happens completely after
			// Trunk <e> in Thread <the_trunk.first>. 
			// Therefore, there are no concurrent trunks in Thread <thr_id>. 
			continue;
		}
		size_t e1 = (it - thr_trunks.begin()) - 1;
		extend_until_enforce(thr_id, s1, e1);
		for (size_t trunk_id = s1; trunk_id <= e1; ++trunk_id)
			concurrent_trunks.push_back(make_pair(thr_id, trunk_id));
	}
}
