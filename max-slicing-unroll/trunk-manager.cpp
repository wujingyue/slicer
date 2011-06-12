#include "common/callgraph-fp/callgraph-fp.h"
using namespace llvm;

#include "trunk-manager.h"
#include "clone-map-manager.h"

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
		CallGraphFP::SiteList call_sites = CG.get_call_sites(f);
		forall(CallGraphFP::SiteList, it, call_sites)
			search_containing_trunks(*it, visited, containing_trunks);
	}

	void TrunkManager::get_concurrent_trunks(
			const pair<int, size_t> &the_trunk,
			vector<pair<int, size_t> > &concurrent_trunks) const {
		assert_not_implemented();
	}
}
