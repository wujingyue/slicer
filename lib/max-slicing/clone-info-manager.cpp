#include "common/callgraph-fp.h"
using namespace llvm;

#include "slicer/clone-info-manager.h"
using namespace slicer;

static RegisterPass<CloneInfoManager> X("clone-info-manager",
		"The CloneInfo manager",
		false, true);

char CloneInfoManager::ID = 0;

void CloneInfoManager::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequiredTransitive<CallGraphFP>();
	ModulePass::getAnalysisUsage(AU);
}

bool CloneInfoManager::runOnModule(Module &M) {
	forallinst(M, ins) {
		if (!has_clone_info(ins))
			continue;
		rmap[get_clone_info(ins)].push_back(ins);
	}
	if (rmap.empty())
		errs() << "[Warning] The program does not contain any clone_info.\n";
	return false;
}

bool CloneInfoManager::has_clone_info() const {
	return rmap.size() > 0;
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
	assert(ci.thr_id >= 0);
	assert(ci.trunk_id != (size_t)-1);
	assert(ci.orig_ins_id != IDManager::INVALID_ID);
	return ci;
}

InstList CloneInfoManager::get_instructions(int thr_id,
		size_t trunk_id, unsigned orig_ins_id) const {
	CloneInfo ci;
	ci.thr_id = thr_id;
	ci.trunk_id = trunk_id;
	ci.orig_ins_id = orig_ins_id;
	DenseMap<CloneInfo, InstList>::const_iterator it = rmap.find(ci);
	if (it == rmap.end())
		return InstList();
	return it->second;
}

Instruction *CloneInfoManager::get_any_instruction(int thr_id) const {
	for (DenseMap<CloneInfo, InstList>::const_iterator it = rmap.begin();
			it != rmap.end(); ++it) {
		if (it->first.thr_id == thr_id) {
			assert(!it->second.empty());
			return it->second.front();
		}
	}
	return NULL;
}
