/**
 * Author: Jingyue
 *
 * Capture loop bounds. 
 */

#define DEBUG_TYPE "int"

#include "llvm/Support/Debug.h"
using namespace llvm;

#include "slicer/capture.h"
using namespace slicer;

bool CaptureConstraints::get_loop_bound(const Loop *L,
		vector<Clause *> &loop_constraints) {
	loop_constraints.clear();

	const PHINode *iv = L->getCanonicalInductionVariable();
	// Give up if we cannot find the loop index. 
	if (!iv) {
		DEBUG(dbgs() << "Unable to find the canonical induction variable.\n";);
		return false;
	}
	if (iv->getNumIncomingValues() != 2) {
		DEBUG(dbgs() << "Unable to compute the bound of" << *iv << "\n";);
		return false;
	}

	// Best case: Already optimized as a loop with a trip count. 
	Constant *zero = ConstantInt::get(int_type, 0);
	if (Value *trip = L->getTripCount()) {
		loop_constraints.push_back(new Clause(new BoolExpr(CmpInst::ICMP_SGE,
					new Expr(iv), new Expr(zero))));
		loop_constraints.push_back(new Clause(new BoolExpr(CmpInst::ICMP_SLT,
					new Expr(iv), new Expr(trip))));
		return true;
	}

	// Hard case. 
	// Borrowed from LLVM.
	bool P0InLoop = L->contains(iv->getIncomingBlock(0));
	BasicBlock *BackedgeBlock = iv->getIncomingBlock(!P0InLoop);
	BranchInst *BI = dyn_cast<BranchInst>(BackedgeBlock->getTerminator());
	if (!BI || !BI->isConditional())
		return false;

	vector<Clause *> loop_body_constraints;
	get_in_loop_body(L, loop_body_constraints);

	for (size_t i = 0; i < loop_body_constraints.size(); ++i) {
		Clause *c = loop_body_constraints[i]->clone();
		replace_with_loop_bound_version(c, L);
		loop_constraints.push_back(c);
	}
	// LB(iv) = iv - 1
	// Will attach the context later. 
	loop_constraints.push_back(new Clause(new BoolExpr(CmpInst::ICMP_EQ,
					new Expr(iv, 0, Expr::LoopBound),
					new Expr(Instruction::Sub,
						new Expr(iv), new Expr(ConstantInt::get(int_type, 1))))));
	// (iv == 0) or (iv > 0 and Condition(iv/LB(iv))
	Value *Condition = BI->getCondition();
	Clause *backedge_cond;
	if (BI->getSuccessor(0) == L->getHeader()) {
		// The true branch goes back to the loop entry. 
		backedge_cond = new Clause(new BoolExpr(CmpInst::ICMP_EQ,
					new Expr(Condition, 0, Expr::LoopBound),
					new Expr(ConstantInt::getTrue(BI->getContext()))));
	} else {
		// The false branch goes back to the loop entry. 
		backedge_cond = new Clause(new BoolExpr(CmpInst::ICMP_EQ,
					new Expr(Condition, 0, Expr::LoopBound),
					new Expr(ConstantInt::getFalse(BI->getContext()))));
	}
	loop_constraints.push_back(new Clause(Instruction::Or,
				new Clause(new BoolExpr(CmpInst::ICMP_EQ,
						new Expr(iv), new Expr(zero))),
				new Clause(Instruction::And,
					new Clause(new BoolExpr(CmpInst::ICMP_SGT,
							new Expr(iv), new Expr(zero))),
					backedge_cond)));

	return true;
}

void CaptureConstraints::get_in_loop_body(const Loop *L,
		vector<Clause *> &loop_body_constraints) {
	LoopInfo &LI = getAnalysis<LoopInfo>(*L->getHeader()->getParent());

	loop_body_constraints.clear();

	for (Loop::block_iterator bi = L->block_begin(); bi != L->block_end(); ++bi) {
		const BasicBlock *bb = *bi;
		// If <bb> is contained in a subloop, ignore it. 
		if (LI.getLoopDepth(bb) != L->getLoopDepth())
			continue;
		forallconst(BasicBlock, ins, *bb) {
			if (is_reachable_integer(ins)) {
				if (Clause *c = get_in_user(ins))
					loop_body_constraints.push_back(c);
			}
		}
	}
}

void CaptureConstraints::get_in_loop(const Loop *L,
		vector<Clause *> &loop_constraints) {
	loop_constraints.clear();
	if (get_loop_bound(L, loop_constraints)) {
		vector<Clause *> loop_body_constraints;
		get_in_loop_body(L, loop_body_constraints);
		loop_constraints.insert(loop_constraints.end(),
				loop_body_constraints.begin(), loop_body_constraints.end());
	}
}

void CaptureConstraints::check_loop(Loop *l, DominatorTree &DT) {
	assert(l->isLCSSAForm(DT));
	assert(l->isLoopSimplifyForm());
	for (Loop::iterator li = l->begin(); li != l->end(); ++li)
		check_loop(*li, DT);
}

void CaptureConstraints::check_loops(Module &M) {
	forallfunc(M, f) {
		if (f->isDeclaration())
			continue;
		LoopInfo &LI = getAnalysis<LoopInfo>(*f);
		DominatorTree &DT = getAnalysis<DominatorTree>(*f);
		for (LoopInfo::iterator lii = LI.begin(); lii != LI.end(); ++lii) {
			Loop *l = *lii;
			check_loop(l, DT);
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
			if (LI.getLoopDepth(bb) == l->getLoopDepth()) {
				// e->context is preserved. 
				e->type = Expr::LoopBound;
			}
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

void CaptureConstraints::attach_context(Clause *c, unsigned context) {
	if (c->be)
		attach_context(c->be, context);
	else if (c->op == Instruction::UserOp1)
		attach_context(c->c1, context);
	else {
		attach_context(c->c1, context);
		attach_context(c->c2, context);
	}
}

void CaptureConstraints::attach_context(BoolExpr *be, unsigned context) {
	attach_context(be->e1, context);
	attach_context(be->e2, context);
}

void CaptureConstraints::attach_context(Expr *e, unsigned context) {
	if (e->type == Expr::SingleDef || e->type == Expr::SingleUse ||
			e->type == Expr::LoopBound) {
		const Value *v = (e->type == Expr::SingleUse ? e->u->get() : e->v);
		if (!is_fixed_integer(v))
			e->context = context;
	} else if (e->type == Expr::Unary) {
		attach_context(e->e1, context);
	} else if (e->type == Expr::Binary) {
		attach_context(e->e1, context);
		attach_context(e->e2, context);
	} else {
		assert_not_supported();
	}
}
