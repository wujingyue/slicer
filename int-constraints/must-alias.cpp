/**
 * Author: Jingyue
 *
 * We compute all alias sets in runOnModule. Therefore, each query becomes
 * a simple find-query to a disjoint-set. Besides, given an LLVM value,
 * we are able to quickly compute its containing alias set. 
 *
 * The above approach should be very time-efficient for each query, but may
 * take too long to pre-compute all the alias sets.
 * An alternative approach is instead of maintaining a disjoint-set, we query
 * BddAliasAnalysis each time we get a query. 
 */

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
		ConstValueList candidates;
		get_all_candidates(M, candidates);
		forall(ConstValueList, it, candidates) {
			const Value *v = *it;
			PointeeType ptt;
			const Value *pt;
			bool ret = get_single_pointee(NULL, v, ptt, pt);
			assert(ret && "How was it put to the candidate set?!");
			ptr_pt[v] = pt;
			pt_ptr[pt].insert(v);
		}
		return false;
	}

	bool MustAlias::fast_must_alias(const Value *v1, const Value *v2) const {
		return ptr_pt.count(v1) && ptr_pt.count(v2) &&
			ptr_pt.lookup(v1) == ptr_pt.lookup(v2);
	}

	void MustAlias::get_all_pointers(
			const Module &M, ConstValueList &pointers) const {
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

	void MustAlias::get_all_candidates(
			const Module &M, ConstValueList &candidates) const {
		// Get all pointers. 
		ConstValueList pointers;
		get_all_pointers(M, pointers);
#ifdef VERBOSE
		errs() << "# of pointers = " << pointers.size() << "\n";
#endif
		// Get all candidates.
		candidates.clear();
		forallconst(ConstValueList, it, pointers) {
			PointeeType ptt;
			const Value *pt;
			if (get_single_pointee(NULL, *it, ptt, pt))
				candidates.push_back(*it);
		}
#ifdef VERBOSE
		errs() << "# of candidates = " << candidates.size() << "\n";
#endif
	}

	void MustAlias::print(raw_ostream &O, const Module *M) const {
		// Print all alias sets. 
		DenseMap<const Value *, ConstValueSet>::const_iterator it, E;
		unsigned set_id = 0;
		for (it = pt_ptr.begin(), E = pt_ptr.end(); it != E; ++it) {
			O << "Set " << set_id << ":\n";
			++set_id;
			forallconst(ConstValueSet, j, it->second) {
				O << "\t";
				print_value(O, *j);
			}
		}
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

	const ConstValueSet *MustAlias::get_alias_set(const Value *v) const {
		const Value *pt = ptr_pt.lookup(v);
		if (!pt)
			return NULL;
		return &(pt_ptr.find(pt)->second);
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
		// If <pt> is an array,
		// <pt> may actually point to any part of the region. 
		// Therefore, we cannot infer that it can only point to one location. 
		if (isa<SequentialType>(pt->getType()))
			return false;
		return true;
	}

	vector<const ConstValueSet *> MustAlias::get_all_alias_sets() const {
		vector<const ConstValueSet *> res;
		DenseMap<const Value *, ConstValueSet>::const_iterator it, E;
		for (it = pt_ptr.begin(), E = pt_ptr.end(); it != E; ++it)
			res.push_back(&(it->second));
		return res;
	}

	char MustAlias::ID = 0;
}
