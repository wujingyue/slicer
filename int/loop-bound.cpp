/**
 * Author: Jingyue
 *
 * Capture loop bounds. 
 */

#define DEBUG_TYPE "int"

#include "llvm/Support/Debug.h"
using namespace llvm;

#include "capture.h"
using namespace slicer;

Clause *CaptureConstraints::construct_bound_constraint(const Value *v,
		const Value *lb, bool lb_inclusive,
		const Value *ub, bool ub_inclusive) {
	Clause *c_lb = new Clause(new BoolExpr(
				(lb_inclusive ? CmpInst::ICMP_SGE : CmpInst::ICMP_SGT),
				new Expr(v), new Expr(lb)));
	Clause *c_ub = new Clause(new BoolExpr(
				(ub_inclusive ? CmpInst::ICMP_SLE : CmpInst::ICMP_SLT),
				new Expr(v), new Expr(ub)));
	return new Clause(Instruction::And, c_lb, c_ub);
}

void CaptureConstraints::get_loop_bound(
		const Loop *L, vector<Clause *> &constraints) {
	LoopInfo &LI = getAnalysis<LoopInfo>(*L->getHeader()->getParent());

	constraints.clear();

	const PHINode *IV = L->getCanonicalInductionVariable();
	// Give up if we cannot find the loop index. 
	if (!IV || IV->getNumIncomingValues() != 2) {
		DEBUG(dbgs() << "Unable to compute the bound of" << *IV << "\n";);
		return;
	}

	// Best case: Already optimized as a loop with a trip count. 
	Constant *zero = ConstantInt::get(int_type, 0);
	if (Value *Trip = L->getTripCount()) {
		Clause *c = construct_bound_constraint(IV, zero, true, Trip, false);
		constraints.push_back(c);
		return;
	}

	// Hard case. 
	// Borrowed from LLVM.
	bool P0InLoop = L->contains(IV->getIncomingBlock(0));
	BasicBlock *BackedgeBlock = IV->getIncomingBlock(!P0InLoop);
	BranchInst *BI = dyn_cast<BranchInst>(BackedgeBlock->getTerminator());
	if (!BI || !BI->isConditional())
		return;

	for (Loop::block_iterator bi = L->block_begin(); bi != L->block_end(); ++bi) {
		const BasicBlock *bb = *bi;
		if (LI.getLoopDepth(bb) != L->getLoopDepth())
			continue;
		forallconst(BasicBlock, ins, *bb) {
			if (is_integer(ins)) {
				Clause *c = get_in_user(ins);
				if (c)
					constraints.push_back(c);
			}
		}
	}
	for (size_t i = 0; i < constraints.size(); ++i)
		replace_with_loop_bound_version(constraints[i], L);
	// LB(IV) = IV - 1
	constraints.push_back(new Clause(new BoolExpr(CmpInst::ICMP_EQ,
					new Expr(IV, Expr::LoopBound),
					new Expr(Instruction::Sub,
						new Expr(IV), new Expr(ConstantInt::get(int_type, 1))))));
	// (IV == 0) or (IV > 0 and Condition(IV/LB(IV))
	Value *Condition = BI->getCondition();
	Clause *backedge_cond;
	if (BI->getSuccessor(0) == L->getHeader()) {
		// The true branch goes back to the loop entry. 
		backedge_cond = new Clause(new BoolExpr(CmpInst::ICMP_EQ,
					new Expr(Condition, Expr::LoopBound),
					new Expr(ConstantInt::getTrue(BI->getContext()))));
	} else {
		// The false branch goes back to the loop entry. 
		backedge_cond = new Clause(new BoolExpr(CmpInst::ICMP_EQ,
					new Expr(Condition, Expr::LoopBound),
					new Expr(ConstantInt::getFalse(BI->getContext()))));
	}
	constraints.push_back(new Clause(Instruction::Or,
				new Clause(new BoolExpr(CmpInst::ICMP_EQ,
						new Expr(IV), new Expr(zero))),
				new Clause(Instruction::And,
					new Clause(new BoolExpr(CmpInst::ICMP_SGT,
							new Expr(IV), new Expr(zero))),
					backedge_cond)));
}

void CaptureConstraints::check_loop(Loop *l) {
	assert(l->isLCSSAForm());
	assert(l->isLoopSimplifyForm());
	for (Loop::iterator li = l->begin(); li != l->end(); ++li)
		check_loop(*li);
}

void CaptureConstraints::check_loops(Module &M) {
	forallfunc(M, f) {
		if (f->isDeclaration())
			continue;
		LoopInfo &LI = getAnalysis<LoopInfo>(*f);
		for (LoopInfo::iterator lii = LI.begin(); lii != LI.end(); ++lii) {
			Loop *l = *lii;
			check_loop(l);
		}
	}
}

void CaptureConstraints::replace_with_loop_bound_version(
		Clause *c, const Loop *l) {
	if (c->be) {
		replace_with_loop_bound_version(c->be, l);
	} else {
		replace_with_loop_bound_version(c->c1, l);
		replace_with_loop_bound_version(c->c2, l);
	}
}

void CaptureConstraints::replace_with_loop_bound_version(
		BoolExpr *be, const Loop *l) {
	replace_with_loop_bound_version(be->e1, l);
	replace_with_loop_bound_version(be->e2, l);
}

void CaptureConstraints::replace_with_loop_bound_version(
		Expr *e, const Loop *l) {
	if (e->type == Expr::SingleDef) {
		if (const Instruction *ins = dyn_cast<Instruction>(e->v)) {
			const BasicBlock *bb = ins->getParent();
			const Function *f = bb->getParent();
			LoopInfo &LI = getAnalysis<LoopInfo>(*const_cast<Function *>(f));
			if (LI.getLoopDepth(bb) == l->getLoopDepth())
				e->type = Expr::LoopBound;
		}
	} else if (e->type == Expr::SingleUse) {
		/* Nothing */
	} else if (e->type == Expr::LoopBound) {
		/* Nothing */
	} else if (e->type == Expr::Unary) {
		replace_with_loop_bound_version(e->e1, l);
	} else if (e->type == Expr::Binary) {
		replace_with_loop_bound_version(e->e1, l);
		replace_with_loop_bound_version(e->e2, l);
	} else {
		assert_not_supported();
	}
}

bool CaptureConstraints::comes_from_shallow(
		const BasicBlock *x, const BasicBlock *y) {
	assert(x->getParent() == y->getParent());
	const Function *f = x->getParent();
	LoopInfo &LI = getAnalysis<LoopInfo>(*const_cast<Function *>(f));
	if (LI.getLoopDepth(x) < LI.getLoopDepth(y))
		return true;
	if (LI.getLoopDepth(x) > LI.getLoopDepth(y))
		return false;
	if (LI.isLoopHeader(const_cast<BasicBlock *>(y))) {
		const Loop *ly = LI.getLoopFor(y);
		assert(ly);
		if (ly->contains(x))
			return false;
	}
	return true;
}
