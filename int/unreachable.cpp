#include "llvm/LLVMContext.h"
#include "common/cfg/intra-reach.h"
using namespace llvm;

#include "config.h"
#include "capture.h"
#include "../max-slicing/max-slicing.h"
using namespace slicer;

void CaptureConstraints::capture_unreachable(Module &M) {
	forallfunc(M, fi) {
		if (!fi->isDeclaration())
			capture_unreachable_in_func(fi);
	}
}

Clause *CaptureConstraints::get_avoid_branch(
		const TerminatorInst *ti, unsigned i) const {
	assert(ti->getNumSuccessors() > 1);
	assert(isa<BranchInst>(ti) || isa<IndirectBrInst>(ti) ||
			isa<InvokeInst>(ti) || isa<SwitchInst>(ti));
	if (const BranchInst *bi = dyn_cast<BranchInst>(ti)) {
		assert(i == 0 || i == 1);
		const Value *cond = bi->getCondition();
		assert(cond && cond->getType()->isIntegerTy(1));
		if (!is_constant(cond))
			return NULL;
		// i == 0 => cond is 1
		// i == 1 => cond is 0
		if (i == 0) {
			return new Clause(new BoolExpr(
						CmpInst::ICMP_EQ,
						new Expr(cond),
						new Expr(ConstantInt::getFalse(getGlobalContext()))));
		} else {
			return new Clause(new BoolExpr(
						CmpInst::ICMP_EQ,
						new Expr(cond),
						new Expr(ConstantInt::getTrue(getGlobalContext()))));
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
			// Case 0 is the default branch. It doesn't have a case value. 
			for (unsigned j = 1; j < si->getNumCases(); ++j) {
				Clause *c = new Clause(new BoolExpr(
							CmpInst::ICMP_EQ, new Expr(cond), new Expr(si->getCaseValue(j))));
				if (!disj)
					disj = c;
				else
					disj = new Clause(Instruction::Or, disj, c);
			}
			assert(disj);
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
		if (MaxSlicing::is_unreachable(bi))
			sink.insert(bi);
	}
	forall(Function, bi, *f) {
		bool already_in_sink = sink.count(bi);
		if (!already_in_sink)
			sink.insert(bi);
		ConstBBSet visited;
		IntraReach &IR = getAnalysis<IntraReach>(*f);
		IR.floodfill(&f->getEntryBlock(), sink, visited);
		// Assume <bi> post-dominates the function entry. 
		bool post_doms = true;
		forall(Function, bj, *f) {
			if (visited.count(bj) && !sink.count(bj) && is_ret(bj->getTerminator())) {
				// Erase it from the set <post_doms> if it doesn't really
				// post-dominate the function entry. 
				post_doms = false;
				break;
			}
		}
		// TODO: It's also possible that a BB's only successor is unreachable,
		// although unlikely. We don't handle this case for now. 
		TerminatorInst *ti = bi->getTerminator();
		if (post_doms && ti->getNumSuccessors() > 1) {
			for (unsigned i = 0; i < ti->getNumSuccessors(); ++i) {
				if (MaxSlicing::is_unreachable(ti->getSuccessor(i))) {
					Clause *c = get_avoid_branch(ti, i);
					if (c)
						constraints.push_back(c);
				}
			}
		}
		if (!already_in_sink)
			sink.erase(bi);
	}
}
