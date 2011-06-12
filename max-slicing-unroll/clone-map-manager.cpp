#include "llvm/Support/CommandLine.h"
#include "idm/id.h"
using namespace llvm;

#include <fstream>
#include <sstream>
using namespace std;

#include "clone-map-manager.h"

namespace {
	// It's a manager pass. It does not need a command line option. 
	static cl::opt<string> MappingFile(
			"clone-map",
			cl::NotHidden,
			cl::desc("Output file containing the clone mapping"),
			cl::init(""));
}

namespace slicer {

	void CloneMapManager::getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
		AU.addRequired<ObjectID>();
		ModulePass::getAnalysisUsage(AU);
	}

	bool CloneMapManager::runOnModule(Module &M) {
		assert(MappingFile != "");
		ifstream fin(MappingFile.c_str());
		string line;
		while (getline(fin, line)) {
			istringstream iss(line);
			int thr_id;
			size_t trunk_id;
			unsigned old_inst_id, new_inst_id;
			if (iss >> thr_id >> trunk_id >> old_inst_id >> new_inst_id) {
				if (thr_id < 0)
					continue;
				ObjectID &OI = getAnalysis<ObjectID>();
				Instruction *old_inst = OI.getInstruction(old_inst_id);
				Instruction *new_inst = OI.getInstruction(new_inst_id);
				assert(old_inst && new_inst);
				clone_map_r[new_inst] = old_inst;
				cloned_to_tid[new_inst] = thr_id;
				cloned_to_trunk[new_inst] = trunk_id;
				while (trunk_id >= clone_map[thr_id].size())
					clone_map[thr_id].push_back(ConstInstMapping());
				clone_map[thr_id][trunk_id][old_inst] = new_inst;
			}
		}
		return false;
	}

	vector<int> CloneMapManager::get_thr_ids() const {
		vector<int> res;
		map<int, vector<ConstInstMapping> >::const_iterator it;
		for (it = clone_map.begin(); it != clone_map.end(); ++it)
			res.push_back(it->first);
		return res;
	}

	size_t CloneMapManager::get_n_trunks(int thr_id) const {
		assert(clone_map.count(thr_id));
		return clone_map.find(thr_id)->second.size();
	}

	const Instruction *CloneMapManager::get_orig_inst(
			const Instruction *new_inst) const {
		return clone_map_r.lookup(new_inst);
	}

	int CloneMapManager::get_thr_id(const Instruction *new_inst) const {
		assert(cloned_to_tid.count(new_inst));
		return cloned_to_tid.lookup(new_inst);
	}

	size_t CloneMapManager::get_trunk_id(const Instruction *new_inst) const {
		assert(cloned_to_trunk.count(new_inst));
		return cloned_to_trunk.lookup(new_inst);
	}

	const Instruction *CloneMapManager::get_cloned_inst(
			int thr_id, size_t trunk_id, const Instruction *old_inst) const {
		if (!clone_map.count(thr_id))
			return NULL;
		const vector<ConstInstMapping> &thr_clone_map =
			clone_map.find(thr_id)->second;
		if (trunk_id >= thr_clone_map.size())
			return NULL;
		return thr_clone_map[trunk_id].lookup(old_inst);
	}

	char CloneMapManager::ID = 0;
}
