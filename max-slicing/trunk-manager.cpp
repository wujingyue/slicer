#include "common/callgraph-fp/callgraph-fp.h"
using namespace llvm;

#include "trunk-manager.h"
#include "clone-map-manager.h"
#include "trace/trace-manager.h"
#include "trace/landmark-trace.h"

namespace {

	static RegisterPass<slicer::TrunkManager> X(
			"trunk-manager",
			"The trunk manager",
			false,
			true);
}

namespace slicer {

	char TrunkManager::ID = 0;

	void TrunkManager::getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
		AU.addRequiredTransitive<CallGraphFP>();
		AU.addRequiredTransitive<CloneMapManager>();
		AU.addRequiredTransitive<TraceManager>();
		AU.addRequiredTransitive<LandmarkTrace>();
		ModulePass::getAnalysisUsage(AU);
	}

	bool TrunkManager::runOnModule(Module &M) {
		return false;
	}

	void TrunkManager::get_containing_trunks(
			const Instruction *ins,
			vector<pair<int, size_t> > &containing_trunks) const {
		containing_trunks.clear();
		ConstInstSet visited;
		search_containing_trunks(ins, visited, containing_trunks);
		sort(containing_trunks.begin(), containing_trunks.end());
		containing_trunks.resize(
				unique(containing_trunks.begin(), containing_trunks.end()) -
				containing_trunks.begin());
	}

	void TrunkManager::search_containing_trunks(
			const Instruction *ins,
			ConstInstSet &visited,
			vector<pair<int, size_t> > &containing_trunks) const {
		if (visited.count(ins))
			return;
		visited.insert(ins);
		CloneMapManager &CMM = getAnalysis<CloneMapManager>();
		int thr_id = CMM.get_thr_id(ins);
		if (thr_id != -1) {
			containing_trunks.push_back(make_pair(thr_id, CMM.get_trunk_id(ins)));
			return;
		}
		// Trace back via the callgraph
		CallGraphFP &CG = getAnalysis<CallGraphFP>();
		const Function *f = ins->getParent()->getParent();
		InstList call_sites = CG.get_call_sites(f);
		forall(InstList, it, call_sites)
			search_containing_trunks(*it, visited, containing_trunks);
	}

	void TrunkManager::extend_until_enforce(
			int thr_id, size_t &s, size_t &e) const {
		LandmarkTrace &LT = getAnalysis<LandmarkTrace>();
		TraceManager &TM = getAnalysis<TraceManager>();
		while (s > 0) {
			unsigned idx = LT.get_landmark(thr_id, s);
			if (TM.get_record_info(idx).type == TR_LANDMARK_ENFORCE)
				break;
			--s;
		}
		// If e == LT.get_n_trunks(thr_id), then e is already the last trunk. 
		while (e + 1 < LT.get_n_trunks(thr_id)) {
			size_t e1 = e + 1;
			unsigned idx = LT.get_landmark(thr_id, e1);
			if (TM.get_record_info(idx).type == TR_LANDMARK_ENFORCE)
				break;
			e = e1;
		}
	}

	void TrunkManager::get_concurrent_trunks(
			const pair<int, size_t> &the_trunk,
			vector<pair<int, size_t> > &concurrent_trunks) const {
		LandmarkTrace &LT = getAnalysis<LandmarkTrace>();
		size_t s = the_trunk.second, e = the_trunk.second;
		extend_until_enforce(the_trunk.first, s, e);
		unsigned s_idx = LT.get_landmark(the_trunk.first, s);
		unsigned e_idx = LT.get_landmark(the_trunk.first,
				(e + 1 == LT.get_n_trunks(the_trunk.first) ? e : e + 1));
		vector<int> thr_ids = LT.get_thr_ids();
		for (size_t i = 0; i < thr_ids.size(); ++i) {
			int thr_id = thr_ids[i];
			if (thr_id == the_trunk.first)
				continue;
			const vector<unsigned> &thr_trunks = LT.get_thr_trunks(thr_id);
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
}
