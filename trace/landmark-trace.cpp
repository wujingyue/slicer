/**
 * Author: Jingyue
 */

#define DEBUG_TYPE "trace"

#include <fstream>
using namespace std;

#include "llvm/Support/CommandLine.h"
#include "llvm/ADT/Statistic.h"
using namespace llvm;

#include "landmark-trace.h"
using namespace slicer;

static RegisterPass<LandmarkTrace> X("manage-landmark-trace",
		"Reads from the landmark trace file, and manages it",
		false, true); // is analysis
static cl::opt<string> LandmarkTraceFile("input-landmark-trace",
		cl::desc("The input landmark trace file"),
		cl::init(""));

STATISTIC(NumEnforcingEvents, "Number of enforcing events");
STATISTIC(NumDerivedEvents, "Number of derived events");

char LandmarkTrace::ID = 0;

void LandmarkTrace::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	ModulePass::getAnalysisUsage(AU);
}

vector<int> LandmarkTrace::get_thr_ids() const {
	map<int, vector<LandmarkTraceRecord> >::const_iterator it;
	vector<int> res;
	for (it = thread_trunks.begin(); it != thread_trunks.end(); ++it)
		res.push_back(it->first);
	return res;
}

bool LandmarkTrace::runOnModule(Module &M) {

	assert(LandmarkTraceFile != "" && "Didn't specify the input landmark trace");
	ifstream fin(LandmarkTraceFile.c_str(), ios::in | ios::binary);
	assert(fin && "Cannot open the input landmark trace file");
	LandmarkTraceRecord record;
	while (fin.read((char *)&record, sizeof record)) {
		thread_trunks[record.tid].push_back(record);
		if (record.enforcing)
			++NumEnforcingEvents;
		else
			++NumDerivedEvents;
	}

	return false;
}

void LandmarkTrace::print(raw_ostream &O, const Module *M) const {
}

size_t LandmarkTrace::get_n_trunks(int thr_id) const {
	return get_thr_trunks(thr_id).size();
}

unsigned LandmarkTrace::get_landmark_timestamp(
		int thr_id, size_t trunk_id)	const {
	return get_landmark(thr_id, trunk_id).idx;
}

const LandmarkTraceRecord &LandmarkTrace::get_landmark(
		int thr_id, size_t trunk_id) const {
	const vector<LandmarkTraceRecord> &thr_trunks = get_thr_trunks(thr_id);
	assert(trunk_id < thr_trunks.size());
	return thr_trunks[trunk_id];
}

void LandmarkTrace::extend_until_enforce(
		int thr_id, size_t &s, size_t &e) const {
	assert(s <= e);
	if (e >= get_n_trunks(thr_id)) {
		errs() << "thr_id = " << thr_id << "\n";
		errs() << "# of trunks = " << get_n_trunks(thr_id) << "\n";
		errs() << "trunk id = " << e << "\n";
	}
	assert(e < get_n_trunks(thr_id));
	while (s > 0) {
		if (is_enforcing_landmark(thr_id, s))
			break;
		--s;
	}
	// If e + 1 == get_n_trunks(thr_id), then e is already the last trunk. 
	while (e + 1 < get_n_trunks(thr_id)) {
		size_t e1 = e + 1;
		if (is_enforcing_landmark(thr_id, e1))
			break;
		e = e1;
	}
}

void LandmarkTrace::get_concurrent_regions(
		const pair<int, size_t> &the_trunk,
		vector<pair<int, pair<size_t, size_t> > > &concurrent_regions) const {
	size_t s = the_trunk.second, e = the_trunk.second;
	extend_until_enforce(the_trunk.first, s, e);
	unsigned s_idx = get_landmark_timestamp(the_trunk.first, s);
	unsigned e_idx = get_landmark_timestamp(the_trunk.first,
			(e + 1 == get_n_trunks(the_trunk.first) ? e : e + 1));
	vector<int> thr_ids = get_thr_ids();
	for (size_t i = 0; i < thr_ids.size(); ++i) {
		int thr_id = thr_ids[i];
		// Look at other threads only. 
		if (thr_id == the_trunk.first)
			continue;
		size_t s1 = search_thr_landmark(thr_id, s_idx);
		if (s1 > 0)
			--s1;
		size_t e1 = search_thr_landmark(thr_id, e_idx);
		if (e1 == 0) {
			// The first landmark in Thread <thr_id> happens completely after
			// Trunk <e> in Thread <the_trunk.first>. 
			// Therefore, there are no concurrent trunks in Thread <thr_id>. 
			continue;
		}
		--e1;
		extend_until_enforce(thr_id, s1, e1);
		concurrent_regions.push_back(make_pair(thr_id, make_pair(s1, e1)));
	}
}

void LandmarkTrace::get_concurrent_trunks(
		const pair<int, size_t> &the_trunk,
		vector<pair<int, size_t> > &concurrent_trunks) const {
	vector<pair<int, pair<size_t, size_t> > > concurrent_regions;
	get_concurrent_regions(the_trunk, concurrent_regions);
	for (size_t i = 0; i < concurrent_regions.size(); ++i) {
		int thr_id = concurrent_regions[i].first;
		size_t s = concurrent_regions[i].second.first;
		size_t e = concurrent_regions[i].second.second;
		for (size_t trunk_id = s; trunk_id <= e; ++trunk_id)
			concurrent_trunks.push_back(make_pair(thr_id, trunk_id));
	}
}

size_t LandmarkTrace::get_latest_happens_before(
		int tid, size_t trunk_id, int tid_2) const {

	assert(trunk_id < get_n_trunks(tid));
	
	/* Find the latest enforincg landmark before Trunk <trunk_id>. */
	size_t s = trunk_id, e = trunk_id;
	extend_until_enforce(tid, s, e);
	/*
	 * Example:
	 *
	 * child_t1        child_t2
	 *
	 * L1
	 *                 Thread function starts.
	 *                 i2
	 *
	 * <i2> is not guaranteed to happen after L1, because the event when
	 * a thread starts is not considered as an enforcing landmark. 
	 */
	if (!is_enforcing_landmark(tid, s))
		return (size_t)-1;
	
	/*
	 * Landmark (tid_2, trunk_id_2) happens right before Landmark (tid, s)
	 * in real time. 
	 */
	unsigned idx = get_landmark_timestamp(tid, s);
	size_t trunk_id_2 = search_thr_landmark(tid_2, idx) - 1;
	if (trunk_id_2 == (size_t)-1)
		return (size_t)-1;
	assert(get_landmark_timestamp(tid_2, trunk_id_2) < idx);
	
	/*
	 * However, that doesn't mean Landmark (tid_2, trunk_id_2) will always happen
	 * before Landmark (tid, s) because it might not be an enforcing landmark. 
	 * Therefore, we search backwards until we reach a enforcing landmark
	 * (tid_2, s_2).
	 */
	size_t s_2 = trunk_id_2, e_2 = trunk_id_2;
	extend_until_enforce(tid_2, s_2, e_2);
	if (!is_enforcing_landmark(tid_2, s_2))
		return (size_t)-1;
	return s_2;
}

bool LandmarkTrace::happens_before(int i1, size_t j1, int i2, size_t j2) const {
	size_t tmp = j1;
	extend_until_enforce(i1, tmp, j1);
	tmp = j2;
	extend_until_enforce(i2, j2, tmp);
	if (j1 + 1 < get_n_trunks(i1))
		++j1;
	if (!is_enforcing_landmark(i1, j1) || !is_enforcing_landmark(i2, j2))
		return false;
	return get_landmark_timestamp(i1, j1) <= get_landmark_timestamp(i2, j2);
}

const vector<LandmarkTraceRecord> &LandmarkTrace::get_thr_trunks(
		int thr_id) const {
	assert(thread_trunks.count(thr_id));
	return thread_trunks.find(thr_id)->second;
}

// Find the first index >= <idx>. 
// <, <, <, >=, >=
size_t LandmarkTrace::search_thr_landmark(int thr_id, unsigned idx) const {
	const vector<LandmarkTraceRecord> &thr_trunks = get_thr_trunks(thr_id);
	size_t low = 0, high = thr_trunks.size();
	while (low < high) {
		size_t mid = (low + high) / 2;
		if (thr_trunks[mid].idx >= idx)
			high = mid;
		else
			low = mid + 1;
	}
	assert(low == high);
	return low;
}

bool LandmarkTrace::is_enforcing_landmark(int thr_id, size_t trunk_id) const {
	return get_landmark(thr_id, trunk_id).enforcing;
}
