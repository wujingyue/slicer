#include "llvm/Support/CommandLine.h"
#include "llvm/Analysis/Dominators.h"
#include "common/include/util.h"
#include "common/cfg/intra-reach.h"
#include "idm/id.h"
using namespace llvm;

#include <sstream>
using namespace std;

#include "capture.h"
#include "solve.h"
using namespace slicer;

static RegisterPass<SolveConstraints> X(
		"solve",
		"Solve captured constraints using STP",
		false,
		true); // is analysis

char SolveConstraints::ID = 0;

SolveConstraints::SolveConstraints(): ModulePass(&ID) {
	vc = NULL;
}

SolveConstraints::~SolveConstraints() {
	errs() << "SolveConstraints::~SolveConstraints\n";
	vc_Destroy(vc);
}

void SolveConstraints::releaseMemory() {
	errs() << "SolveConstraints::releaseMemory\n";
}

bool SolveConstraints::runOnModule(Module &M) {
	errs() << "SolveConstraints::runOnModule\n";
	return recalculate(M);
}

bool SolveConstraints::recalculate(Module &M) {

	// Remove the old validity checker. 
	if (vc)
		vc_Destroy(vc);
	vc = vc_createValidityChecker();
	// vc_setFlags(vc, 'p');
	vc_registerErrorHandler(vc_error_handler);

	// Translate captured constraints to the VC form. 
	translate_captured();
	// The captured constraints should be consistent. 
	vc_push(vc);
	assert(vc_query(vc, vc_falseExpr(vc)) == 0 &&
			"The captured constraints is not consistent.");
	vc_pop(vc);

	return false;
}

void SolveConstraints::translate_captured() {
	CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
#if 0
	for (unsigned i = 0; i < CC.get_num_constraints(); ++i) {
		const Clause *c = CC.get_constraint(i);
		VCExpr vc_expr = translate_to_vc(c);
		vc_assertFormula(vc, vc_expr);
	}
#endif
	root.clear();
	for (unsigned i = 0; i < CC.get_num_constraints(); ++i) {
		const Clause *c = CC.get_constraint(i);
		const Value *v1 = NULL, *v2 = NULL;
		if (is_simple_eq(c, v1, v2)) {
			assert(v1 && v2);
			const Value *r1 = get_root(v1), *r2 = get_root(v2);
			root[r1] = r2;
		}
	}
	forallconst(ConstValueMapping, it, root) {
		const Value *v1 = it->first;
		const Value *v2 = it->second;
		if (v1 != v2) {
			const Clause *c = new Clause(new BoolExpr(
						CmpInst::ICMP_EQ, new Expr(v1), new Expr(v2)));
			vc_assertFormula(vc, translate_to_vc(c));
			delete c;
		}
	}
	for (unsigned i = 0; i < CC.get_num_constraints(); ++i) {
		Clause *c2 = CC.get_constraint(i)->clone();
		replace_with_root(c2);
		vc_assertFormula(vc, translate_to_vc(c2));
		delete c2;
	}
}

void SolveConstraints::replace_with_root(Clause *c) {
	if (c->be)
		replace_with_root(c->be);
	else {
		replace_with_root(c->c1);
		replace_with_root(c->c2);
	}
}

void SolveConstraints::replace_with_root(BoolExpr *be) {
	replace_with_root(be->e1);
	replace_with_root(be->e2);
}

void SolveConstraints::replace_with_root(Expr *e) {
	if (e->type == Expr::SingleDef)
		e->v = get_root(e->v);
	else if (e->type == Expr::Unary)
		replace_with_root(e->e1);
	else if (e->type == Expr::Binary) {
		replace_with_root(e->e1);
		replace_with_root(e->e2);
	}
}

const Value *SolveConstraints::get_root(const Value *x) {
	assert(x);
	if (!root.count(x))
		return x;
	const Value *y = root.lookup(x);
	if (y != x) {
		const Value *ry = get_root(y);
		root[x] = ry;
	}
	return root.lookup(x);
}

bool SolveConstraints::is_simple_eq(
		const Clause *c, const Value *&v1, const Value *&v2) {
	if (c->be == NULL)
		return false;
	if (c->be->p != CmpInst::ICMP_EQ)
		return false;
	if (c->be->e1->type != Expr::SingleDef || c->be->e2->type != Expr::SingleDef)
		return false;
	v1 = c->be->e1->v;
	v2 = c->be->e2->v;
	return true;
}

void SolveConstraints::vc_error_handler(const char *err_msg) {
	errs() << "Error in VC: ";
	errs() << err_msg << "\n";
}

VCExpr SolveConstraints::translate_to_vc(const Clause *c) {
	if (c->be)
		return translate_to_vc(c->be);
	VCExpr vce1 = translate_to_vc(c->c1);
	VCExpr vce2 = translate_to_vc(c->c2);
	if (c->op == Instruction::And)
		return vc_andExpr(vc, vce1, vce2);
	else if (c->op == Instruction::Or)
		return vc_orExpr(vc, vce1, vce2);
	else
		return vc_xorExpr(vc, vce1, vce2);
}

VCExpr SolveConstraints::translate_to_vc(const BoolExpr *be) {
	const Expr *e1 = be->e1, *e2 = be->e2;
	assert(e1->get_width() == e2->get_width());
	VCExpr vce1 = translate_to_vc(e1);
	VCExpr vce2 = translate_to_vc(e2);
	switch (be->p) {
		case CmpInst::ICMP_EQ:
			return vc_eqExpr(vc, vce1, vce2);
		case CmpInst::ICMP_NE:
			return vc_notExpr(vc, vc_eqExpr(vc, vce1, vce2));
		case CmpInst::ICMP_UGT:
		case CmpInst::ICMP_SGT:
			return vc_sbvGtExpr(vc, vce1, vce2);
		case CmpInst::ICMP_UGE:
		case CmpInst::ICMP_SGE:
			return vc_sbvGeExpr(vc, vce1, vce2);
		case CmpInst::ICMP_ULT:
		case CmpInst::ICMP_SLT:
			return vc_sbvLtExpr(vc, vce1, vce2);
		case CmpInst::ICMP_ULE:
		case CmpInst::ICMP_SLE:
			return vc_sbvLeExpr(vc, vce1, vce2);
		default: assert(false && "Invalid predicate");
	}
}

VCExpr SolveConstraints::translate_to_vc(const Expr *e) {
	if (e->type == Expr::SingleDef)
		return translate_to_vc(e->v);
	if (e->type == Expr::SingleUse)
		return translate_to_vc(e->u);
	if (e->type == Expr::Unary) {
		VCExpr child = translate_to_vc(e->e1);
		if (e->op == Instruction::SExt) {
			assert(e->e1->get_width() == 1);
			return vc_bvSignExtend(vc, child, 32);
		}
		if (e->op == Instruction::ZExt) {
			assert(e->e1->get_width() == 1);
			// STP does not have bvUnsignExtend
			return vc_bvConcatExpr(
					vc,
					vc_bvConstExprFromInt(vc, 31, 0),
					child);
		}
		if (e->op == Instruction::Trunc) {
			assert(e->e1->get_width() == 32);
			return vc_bvExtract(vc, child, 0, 0);
		}
		assert_not_supported();
	}
	if (e->type == Expr::Binary) {
		VCExpr left = translate_to_vc(e->e1);
		VCExpr right = translate_to_vc(e->e2);
		avoid_overflow(e->op, left, right);
		switch (e->op) {
			case Instruction::Add:
				return vc_bv32PlusExpr(vc, left, right);
			case Instruction::Sub:
				return vc_bv32MinusExpr(vc, left, right);
			case Instruction::Mul:
				return vc_bv32MultExpr(vc, left, right);
			case Instruction::UDiv:
			case Instruction::SDiv:
				// FIXME: sbvDiv doesn't work. 
				return vc_bvDivExpr(vc, 32, left, right);
			case Instruction::URem:
			case Instruction::SRem:
				// FIXME: sbvRem doesn't work. 
				return vc_bvModExpr(vc, 32, left, right);
			case Instruction::Shl:
				// left << right
				return vc_bvVar32LeftShiftExpr(vc, right, left);
			case Instruction::LShr:
			case Instruction::AShr:
				// left >> right
				return vc_bvVar32RightShiftExpr(vc, right, left);
			case Instruction::And:
				return vc_bvAndExpr(vc, left, right);
			case Instruction::Or:
				return vc_bvOrExpr(vc, left, right);
			case Instruction::Xor:
				return vc_bvXorExpr(vc, left, right);
			default: assert_not_supported();
		}
	}
	assert(false && "Invalid expression type");
}

VCExpr SolveConstraints::translate_to_vc(const Value *v) {
	if (const ConstantInt *ci = dyn_cast<ConstantInt>(v)) {
		if (ci->getType()->getBitWidth() == 1)
			return (ci->isOne() ?
					vc_boolToBVExpr(vc, vc_trueExpr(vc)) :
					vc_boolToBVExpr(vc, vc_falseExpr(vc)));
		else
			return vc_bv32ConstExprFromInt(vc, ci->getSExtValue());
	}
	ObjectID &OI = getAnalysis<ObjectID>();
	unsigned value_id = OI.getValueID(v);
	assert(value_id != ObjectID::INVALID_ID);
	ostringstream oss;
	oss << "x" << value_id;
	VCType vct;
	if (v->getType()->isIntegerTy(1))
		vct = vc_bvType(vc, 1);
	else
		vct = vc_bv32Type(vc);
	return vc_varExpr(vc, oss.str().c_str(), vct);
}

VCExpr SolveConstraints::translate_to_vc(const Use *u) {
	return translate_to_vc(u->get());
}

void SolveConstraints::print(raw_ostream &O, const Module *M) const {
	// Don't know what to do. 
}

void SolveConstraints::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequiredTransitive<ObjectID>();
	AU.addRequiredTransitive<DominatorTree>();
	AU.addRequiredTransitive<IntraReach>();
	AU.addRequiredTransitive<CaptureConstraints>();
	ModulePass::getAnalysisUsage(AU);
}

bool SolveConstraints::satisfiable(
		const vector<const Clause *> &more_clauses) {
	vc_push(vc);
	forallconst(vector<const Clause *>, it, more_clauses)
		realize(*it);
	for (size_t i = 0; i < more_clauses.size(); ++i) {
		const Clause *c = more_clauses[i];
		vc_assertFormula(vc, translate_to_vc(c));
	}
	int ret = vc_query(vc, vc_falseExpr(vc));
	assert(ret != 2);
	vc_pop(vc);
	return ret == 0;
}

bool SolveConstraints::provable(
		const vector<const Clause *> &more_clauses) {
	vc_push(vc);
	forallconst(vector<const Clause *>, it, more_clauses)
		realize(*it);
	VCExpr conj = vc_trueExpr(vc);
	forallconst(vector<const Clause *>, it, more_clauses) {
		const Clause *c = *it;
		conj = vc_andExpr(vc, conj, translate_to_vc(c));
	}
	int ret = vc_query(vc, conj);
	vc_pop(vc);
	return ret == 1;
}

void SolveConstraints::realize(const Clause *c) {
	if (c->be)
		realize(c->be);
	else {
		realize(c->c1);
		realize(c->c2);
	}
}

void SolveConstraints::realize(const BoolExpr *be) {
	realize(be->e1);
	realize(be->e2);
}

void SolveConstraints::realize(const Expr *e) {
	if (e->type == Expr::Unary) {
		realize(e->e1);
	} else if (e->type == Expr::Binary) {
		realize(e->e1);
		realize(e->e2);
	} else if (e->type == Expr::SingleUse) {
		realize(e->u);
	} else {
		realize(dyn_cast<Instruction>(e->v));
	}
}

BasicBlock *SolveConstraints::get_idom(BasicBlock *bb) {
	DominatorTree &DT = getAnalysis<DominatorTree>(*bb->getParent());
	DomTreeNode *node = DT[bb];
	DomTreeNode *idom = node->getIDom();
	return (idom ? idom->getBlock() : NULL);
}

void SolveConstraints::realize(const Use *u) {
	// The value of a llvm::Constant is compile-time known. Therefore,
	// we don't need to capture extra constraints. 
	realize(dyn_cast<Instruction>(u->getUser()));
}

void SolveConstraints::realize(const Instruction *ins) {
	if (!ins)
		return;
	BasicBlock *bb = const_cast<BasicBlock *>(ins->getParent());
	Function *f = bb->getParent();
	IntraReach &IR = getAnalysis<IntraReach>(*f);
	while (bb != &f->getEntryBlock()) {
		BasicBlock *p = get_idom(bb);
		assert(p);
		/*
		 * If a successor of <p> cannot reach <bb>, the condition that leads
		 * to that successor must not hold. 
		 */ 
		TerminatorInst *ti = p->getTerminator();
		for (unsigned i = 0; i < ti->getNumSuccessors(); ++i) {
			if (!IR.reachable(ti->getSuccessor(i), bb)) {
				CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
				const Clause *c = CC.get_avoid_branch(ti, i);
				if (c) {
#if 0
					errs() << "[realize] ";
					print_clause(errs(), c, getAnalysis<ObjectID>());
					errs() << "\n";
#endif
					VCExpr vce = translate_to_vc(c);
					vc_assertFormula(vc, vce);
					delete c;
				}
			}
		}
		bb = p;
	}
}

void SolveConstraints::avoid_overflow(
		unsigned op, VCExpr left, VCExpr right) {
	switch (op) {
		case Instruction::Add:
			// TODO: Not sure which one is faster. 
			{
				VCExpr long_sum = vc_bvPlusExpr(
						vc, 64,
						vc_bvSignExtend(vc, left, 64), vc_bvSignExtend(vc, right, 64));
				vc_assertFormula(
						vc, vc_sbvLeExpr(vc, vc_int_min_64(vc), long_sum));
				vc_assertFormula(
						vc, vc_sbvLeExpr(vc, long_sum, vc_int_max_64(vc)));
			}
#if 0
			// -oo <= left + right <= oo
			// left >= 0: right <= oo - left
			// left < 0: right >= -oo - left
			vc_assertFormula(
					vc,
					vc_impliesExpr(
						vc,
						vc_sbvGeExpr(vc, left, vc_zero(vc)),
						vc_sbvLeExpr(
							vc,
							right,
							vc_bv32MinusExpr(vc, vc_int_max(vc), left))));
			vc_assertFormula(
					vc,
					vc_impliesExpr(
						vc,
						vc_sbvLtExpr(vc, left, vc_zero(vc)),
						vc_sbvGeExpr(
							vc,
							right,
							vc_bv32MinusExpr(vc, vc_int_min(vc), left))));
#endif
			break;
		case Instruction::Sub:
			// -oo <= left + (-right) <= oo
			avoid_overflow(
					Instruction::Add, left, vc_bvUMinusExpr(vc, right));
			break;
		case Instruction::Mul:
			// TODO: does not support negative numbers
			/*
			 * FIXME: impliesExpr doesn't work as expected. 
			 * We expected to use it to get around the "div-by-zero" problem.
			 * Therefore, we extend the operands to 64-bit integers, and check
			 * whether the product is really out of range. 
			 */
#if 0
			// left >= 0, right >= 0, left * right <= oo
			{
				vc_assertFormula(vc, vc_sbvGeExpr(vc, left, vc_zero(vc)));
				vc_assertFormula(vc, vc_sbvGeExpr(vc, right, vc_zero(vc)));
				VCExpr long_product = vc_bvMultExpr(
						vc, 64,
						vc_bvSignExtend(vc, left, 64), vc_bvSignExtend(vc, right, 64));
				vc_assertFormula(
						vc, vc_sbvLeExpr(vc, long_product, vc_int_max_64(vc)));
			}
			// left > 0 => right <= oo / left
			vc_assertFormula(
					vc,
					vc_impliesExpr(
						vc,
						vc_sbvGtExpr(vc, left, vc_zero(vc)),
						vc_sbvLeExpr(
							vc,
							right,
							vc_sbvDivExpr(vc, 32, vc_int_max(vc), left))));
			// right > 0 => left <= oo / right
			vc_assertFormula(
					vc,
					vc_impliesExpr(
						vc,
						vc_sbvGtExpr(vc, right, vc_zero(vc)),
						vc_sbvLeExpr(
							vc,
							left,
							vc_sbvDivExpr(vc, 32, vc_int_max(vc), right))));
#endif
			break;
		case Instruction::UDiv:
		case Instruction::SDiv:
			// TODO: assume the divisor > 0
			vc_assertFormula(vc, vc_sbvGtExpr(vc, right, vc_zero(vc)));
			break;
		case Instruction::URem:
		case Instruction::SRem:
			// TODO: assume the divisor > 0
			vc_assertFormula(vc, vc_sbvGtExpr(vc, right, vc_zero(vc)));
			break;
	}
}
