#include "common/cfg/intra-reach.h"
using namespace llvm;

#include "config.h"
#include "capture.h"
using namespace slicer;

void CaptureConstraints::capture_unreachable(Module &M) {
	forallfunc(M, fi)
		capture_unreachable_in_func(fi);
}

bool CaptureConstraints::is_unreachable(const BasicBlock *bb) {
	return isa<UnreachableInst>(bb->getTerminator());
}

const Clause *CaptureConstraints::get_avoid_branch(
		const TerminatorInst *ti, unsigned i) const {
	assert(ti->getNumSuccessors() > 1);
	assert(isa<BranchInst>(ti) || isa<IndirectBrInst>(ti) ||
			isa<InvokeInst>(ti) || isa<SwitchInst>(ti));
	if (const BranchInst *bi = dyn_cast<BranchInst>(ti)) {
		assert(i == 0 || i == 1);
		const Value *cond = bi->getCondition();
		assert(cond);
		if (!is_constant(cond))
			return NULL;
		// i == 0 => cond is 1
		// i == 1 => cond is 0
		if (i == 0) {
			return new Clause(new BoolExpr(
						CmpInst::ICMP_EQ,
						new Expr(cond),
						new Expr(ConstantInt::get(int_type, 0))));
		} else {
			return new Clause(new BoolExpr(
						CmpInst::ICMP_EQ,
						new Expr(cond),
						new Expr(ConstantInt::get(int_type, 1))));
		}
	} else if (const SwitchInst *si = dyn_cast<SwitchInst>(ti)) {
		/*
		 * switch (condition) {
		 *   case 0:
		 *   case 1:
		 *   ...
		 *   case k:
		 *   default:
		 * }
		 */
		const Value *cond = si->getCondition();
		assert(cond);
		if (!is_constant(cond))
			return NULL;
		if (ti->getSuccessor(i) == si->getDefaultDest()) {
			// The condition is equal to one of the case values. 
			Clause *disj = NULL;
			for (unsigned j = 0; j < si->getNumCases(); ++j) {
				Clause *c = new Clause(new BoolExpr(
							CmpInst::ICMP_EQ, new Expr(cond), new Expr(si->getCaseValue(j))));
				if (!disj)
					disj = c;
				else
					disj = new Clause(Instruction::Or, disj, c);
			}
			assert(!disj);
			return disj;
		} else {
			// The condition does not equal the particular case. 
			return new Clause(new BoolExpr(
						CmpInst::ICMP_NE,
						new Expr(cond),
						new Expr(si->getCaseValue(i))));
		}
	} else if (const IndirectBrInst *ii = dyn_cast<IndirectBrInst>(ti)) {
		errs() << *ii << "\n";
		assert_not_supported();
	} else {
		return NULL;
	}
}

void CaptureConstraints::capture_unreachable_in_func(Function *f) {
#ifdef VERBOSE
	errs() << "capture_unreachable: " << f->getNameStr() << "\n";
#endif
	/*
	 * If <bb> post-dominates <f> and one of its branches points to an
	 * unreachable BB, the branch condition must be false. 
	 */
	/*
	 * TODO: We could make it more sophisticated: For each path leads to an
	 * unreachable BB, at least one of the conditions along the path must be
	 * false. 
	 */
	/*
	 * Find all BBs that post-dominates the function entry. 
	 * We cannot use PostDominatorTree directly, because we don't want to
	 * count in unreachable BBs. 
	 */
	// TODO: We could make it faster. For now, we use an O(n^2) approach. 
	ConstBBSet sink;
	forall(Function, bi, *f) {
		if (is_unreachable(bi))
			sink.insert(bi);
	}
	ConstBBSet post_doms;
	forall(Function, bi, *f) {
		bool already_in_sink = sink.count(bi);
		if (!already_in_sink)
			sink.insert(bi);
		ConstBBSet visited;
		IntraReach &IR = getAnalysis<IntraReach>(*f);
		IR.floodfill(&f->getEntryBlock(), sink, visited);
		// Assume <bi> post-dominates the function entry. 
		// Will erase it if it doesn't. 
		post_doms.insert(bi);
		forall(Function, bj, *f) {
			if (visited.count(bj) && !sink.count(bj) && is_ret(bj->getTerminator())) {
				// Erase it from the set <post_doms> if it doesn't really
				// post-dominate the function entry. 
				post_doms.erase(bi);
				break;
			}
		}
		if (!already_in_sink)
			sink.erase(bi);
	}
#ifdef VERBOSE
	errs() << "\tBBs that post-dominates the function entry:\n\t";
	forall(ConstBBSet, it, post_doms)
		errs() << (*it)->getNameStr() << " ";
	errs() << "\n";
#endif
	forall(Function, bi, *f) {
		if (!isa<UnreachableInst>(bi->getTerminator()))
			continue;
		// <bi> is an unreachable BB. 

	}
}