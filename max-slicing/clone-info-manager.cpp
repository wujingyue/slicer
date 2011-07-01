#include "common/callgraph-fp/callgraph-fp.h"
using namespace llvm;

#include "clone-info-manager.h"
using namespace slicer;

static RegisterPass<CloneInfoManager> X(
		"trunk-manager",
		"The trunk manager",
		false,
		true);

char CloneInfoManager::ID = 0;

void CloneInfoManager::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequiredTransitive<CallGraphFP>();
	ModulePass::getAnalysisUsage(AU);
}

bool CloneInfoManager::runOnModule(Module &M) {
	/* TODO: Mapping from old instructions to cloned instructions. */
	return false;
}

void CloneInfoManager::get_containing_trunks(
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

bool CloneInfoManager::has_clone_info(const Instruction *ins) const {
	return (ins->getMetadata("clone_info") != NULL);
}

CloneInfo CloneInfoManager::get_clone_info(const Instruction *ins) const {
	MDNode *node = ins->getMetadata("clone_info");
	assert(node && "<ins> does not have any clone info.");
	/* A clone_info metadata node always has 3 ConstantInt operands. */
	assert(node->getNumOperands() == 3);
	for (unsigned i = 0; i < node->getNumOperands(); ++i)
		assert(isa<ConstantInt>(node->getOperand(i)));
	CloneInfo ci;
	ci.thr_id = dyn_cast<ConstantInt>(node->getOperand(0))->getSExtValue();
	ci.trunk_id = dyn_cast<ConstantInt>(node->getOperand(1))->getZExtValue();
	ci.orig_ins_id = dyn_cast<ConstantInt>(node->getOperand(2))->getZExtValue();
	return ci;
}

void CloneInfoManager::search_containing_trunks(
		const Instruction *ins,
		ConstInstSet &visited,
		vector<pair<int, size_t> > &containing_trunks) const {
	if (visited.count(ins))
		return;
	visited.insert(ins);
	/*
	 * If <ins> is in the cloned part (i.e. it has metadata "clone_info),
	 * we take the thread ID and the trunk ID right away. 
	 * Otherwise, we trace back to the caller(s) of the containing function. 
	 */
	if (has_clone_info(ins)) {
		CloneInfo ci = get_clone_info(ins);
		containing_trunks.push_back(make_pair(ci.thr_id, ci.trunk_id));
		return;
	}
	// Trace back via the callgraph
	CallGraphFP &CG = getAnalysis<CallGraphFP>();
	const Function *f = ins->getParent()->getParent();
	InstList call_sites = CG.get_call_sites(f);
	forall(InstList, it, call_sites)
		search_containing_trunks(*it, visited, containing_trunks);
}
