#include "bc2bdd/BddAliasAnalysis.h"
using namespace repair;

#include "must-alias.h"
#include "exec-once.h"

namespace {
	static RegisterPass<slicer::MustAlias> X(
			"must-alias",
			"Must-aliasing",
			false,
			true);
}

namespace slicer {

	void MustAlias::getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
		// Used in <print>. 
		AU.addRequiredTransitive<ExecOnce>();
		AU.addRequiredTransitive<BddAliasAnalysis>();
		ModulePass::getAnalysisUsage(AU);
	}

	bool MustAlias::runOnModule(Module &M) {
		return false;
	}

	void MustAlias::get_all_pointers(
			const Module &M, vector<const Value *> &pointers) const {
		pointers.clear();
		for (Module::const_global_iterator gi = M.global_begin();
				gi != M.global_end(); ++gi) {
			if (isa<PointerType>(gi->getType()))
				pointers.push_back(gi);
		}
		forallconst(Module, fi, M) {
			for (Function::const_arg_iterator ai = fi->arg_begin();
					ai != fi->arg_end(); ++ai) {
				if (isa<PointerType>(ai->getType()))
					pointers.push_back(ai);
			}
			forallconst(Function, bi, *fi) {
				forallconst(BasicBlock, ii, *bi) {
					if (isa<PointerType>(ii->getType()))
						pointers.push_back(ii);
				}
			}
		}
	}

	void MustAlias::print(raw_ostream &O, const Module *M) const {
		// Get all pointers. 
		vector<const Value *> pointers;
		get_all_pointers(*M, pointers);
		O << "# of pointers = " << pointers.size() << "\n";
		// Get all pointers that can point to only one value. 
		BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
		vector<const Value *> candidates;
		forall(vector<const Value *>, it, pointers) {
			const Value *v = *it;
			bdd pointees = BAA.getPointeeSet(NULL, v, 0);
			if (BAA.getPointeeSetSize(pointees) == 1)
				candidates.push_back(v);
		}
		O << "# of candidates = " << candidates.size() << "\n";
	}

	char MustAlias::ID = 0;
}
