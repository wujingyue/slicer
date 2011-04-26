#include "bc2bdd/BddAliasAnalysis.h"
using namespace repair;

#include "must-alias.h"
#include "exec-once.h"
#include "config.h"

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
#ifdef VERBOSE
			print_value(O, *it);
			O << "\tsize = " << BAA.getPointeeSetSize(pointees) << "\n";
#endif
			if (BAA.getPointeeSetSize(pointees) == 1)
				candidates.push_back(v);
		}
		O << "# of candidates = " << candidates.size() << "\n";
		// Dump all candidates
		forall(vector<const Value *>, it, candidates)
			print_value(O, *it);
	}

	void MustAlias::print_value(raw_ostream &O, const Value *v) {
		if (isa<GlobalVariable>(v))
			O << "[global] ";
		else if (const Argument *arg = dyn_cast<Argument>(v))
			O << "[arg] (" << arg->getParent()->getNameStr() << ") ";
		else if (const Instruction *ins = dyn_cast<Instruction>(v))
			O << "[inst] (" << ins->getParent()->getParent()->getNameStr() << "."
				<< ins->getParent()->getNameStr() << ") ";
		else
			assert(false && "Not supported");
		v->print(O);
		O << "\n";
	}


	bool MustAlias::must_alias(
			vector<User *> *ctxt1, const Value *v1,
			vector<User *> *ctxt2, const Value *v2) const {
		PointeeType ptt1, ptt2;
		const Value *pt1, *pt2;
		if (!get_single_pointee(ctxt1, v1, ptt1, pt1) ||
				!get_single_pointee(ctxt1, v2, ptt2, pt2))
			return false;
		// Does not point to the same thing. 
		if (ptt1 != ptt2 || pt1 != pt2)
			return false;
		// Can the pointee correspond to multiple dynamic locations? 
		// A global variable can be allocated only once. 
		if (ptt1 == GLOBAL_VAR)
			return true;
		ExecOnce &EO = getAnalysis<ExecOnce>();
		const Instruction *ins = dyn_cast<Instruction>(pt1);
		assert(ins && "Stack locations and heap locations should be allocated "
				"by instructions");
		if (EO.executed_once(ins))
			return true;
		else
			return false;
	}

	bool MustAlias::must_alias(const Value *v1, const Value *v2) const {
		return must_alias(NULL, v1, NULL, v2);
	}

	bool MustAlias::get_single_pointee(
			vector<User *> *ctxt, const Value *v,
			PointeeType &ptt, const Value *&pt) const {
		BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
		bdd pointees = BAA.getPointeeSet(ctxt, v, 0);
		if (BAA.getPointeeSetSize(pointees) != 1)
			return false;
		BddAliasAnalysis::EnumeratedPointeeSet epts;
		BAA.enumeratePointeeSet(pointees, epts);
		assert(epts.globalLocs.size() + epts.stackLocs.size() +
				epts.heapLocs.size() == 1);
		if (!epts.globalLocs.empty()) {
			ptt = GLOBAL_VAR;
			pt = *epts.globalLocs.begin();
		} else if (!epts.stackLocs.empty()) {
			ptt = STACK_LOC;
			pt = *epts.stackLocs.begin();
		} else {
			ptt = HEAP_LOC;
			pt = *epts.heapLocs.begin();
		}
		// If <pt> is an array, <pt> may actually point to any part of the array.
		// Therefore, we cannot infer that it can only point to one location. 
		if (isa<ArrayType>(pt->getType()))
			return false;
		return true;
	}

	char MustAlias::ID = 0;
}
