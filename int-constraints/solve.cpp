#include "llvm/Support/CommandLine.h"
#include "llvm/Analysis/Dominators.h"
#include "common/include/util.h"
#include "common/reach/intra-reach.h"
#include "idm/id.h"
using namespace llvm;

#include <sstream>
using namespace std;

#include "capture.h"
#include "solve.h"

namespace {

	static RegisterPass<slicer::SolveConstraints> X(
			"solve-constraints",
			"Solve captured constraints using STP",
			false,
			true); // is analysis
}

namespace slicer {

	SolveConstraints::SolveConstraints(): ModulePass(&ID) {
		vc = NULL;
	}

	SolveConstraints::~SolveConstraints() {
		vc_Destroy(vc);
	}

	bool SolveConstraints::runOnModule(Module &M) {
		
		if (vc) {
			vc_Destroy(vc);
			vc = NULL;
		}
		vc = vc_createValidityChecker();
		// vc_setFlags(vc, 'p');
		vc_registerErrorHandler(vc_error_handler);

		CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
		for (unsigned i = 0; i < CC.get_num_constraints(); ++i) {
			const Clause *c = CC.get_constraint(i);
			VCExpr vc_expr = translate_to_vc(c);
			vc_assertFormula(vc, vc_expr);
		}

		return false;
	}

	void SolveConstraints::vc_error_handler(const char *err_msg) {
		errs() << err_msg << "\n";
	}

	bool SolveConstraints::may_equal(const Value *v1, const Value *v2) {
		const Clause *c = new Clause(new BoolExpr(
					CmpInst::ICMP_EQ,
					new Expr(v1),
					new Expr(v2)));
		bool ret = satisfiable(vector<const Clause *>(1, c));
		delete c;
		return ret;
	}

	bool SolveConstraints::must_equal(const Value *v1, const Value *v2) {
		const Clause *c = new Clause(new BoolExpr(
					CmpInst::ICMP_EQ,
					new Expr(v1),
					new Expr(v2)));
		bool ret = provable(vector<const Clause *>(1, c));
		delete c;
		return ret;
	}

	VCExpr SolveConstraints::translate_to_vc(const Clause *c) {
		if (c->be)
			return translate_to_vc(c->be);
		VCExpr vce1 = translate_to_vc(c->c1);
		VCExpr vce2 = translate_to_vc(c->c2);
		if (c->op == Instruction::And)
			return vc_andExpr(vc, vce1, vce2);
		else
			return vc_orExpr(vc, vce1, vce2);
	}

	VCExpr SolveConstraints::translate_to_vc(const BoolExpr *be) {
		VCExpr vce1 = translate_to_vc(be->e1);
		VCExpr vce2 = translate_to_vc(be->e2);
		switch (be->p) {
			case CmpInst::ICMP_EQ:
				return vc_eqExpr(vc, vce1, vce2);
			case CmpInst::ICMP_NE:
				return vc_notExpr(vc, vc_eqExpr(vc, vce1, vce2));
			case CmpInst::ICMP_UGT:
			case CmpInst::ICMP_SGT:
				return vc_bvGtExpr(vc, vce1, vce2);
			case CmpInst::ICMP_UGE:
			case CmpInst::ICMP_SGE:
				return vc_bvGeExpr(vc, vce1, vce2);
			case CmpInst::ICMP_ULT:
			case CmpInst::ICMP_SLT:
				return vc_bvLtExpr(vc, vce1, vce2);
			case CmpInst::ICMP_ULE:
			case CmpInst::ICMP_SLE:
				return vc_bvLeExpr(vc, vce1, vce2);
			default: assert(false && "Invalid predicate");
		}
	}

	VCExpr SolveConstraints::translate_to_vc(const Expr *e) {
		if (e->type == Expr::SingleDef)
			return translate_to_vc(e->v);
		if (e->type == Expr::SingleUse)
			return translate_to_vc(e->u);
		if (e->type == Expr::Unary)
			assert_not_supported();
		if (e->type == Expr::Binary) {
			VCExpr left = translate_to_vc(e->e1);
			VCExpr right = translate_to_vc(e->e2);
			switch (e->op) {
				case Instruction::Add:
					return vc_bv32PlusExpr(vc, left, right);
				case Instruction::Sub:
					return vc_bv32MinusExpr(vc, left, right);
				case Instruction::Mul:
					return vc_bv32MultExpr(vc, left, right);
				case Instruction::UDiv:
				case Instruction::SDiv:
					// TODO: not sure why sbvDivExpr not working. 
					vc_assertFormula(vc,
							vc_bvGtExpr(vc, right, vc_bv32ConstExprFromInt(vc, 0)));
					return vc_sbvDivExpr(vc, 32, left, right);
				case Instruction::URem:
				case Instruction::SRem:
					vc_assertFormula(vc,
							vc_bvGtExpr(vc, right, vc_bv32ConstExprFromInt(vc, 0)));
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
		if (const ConstantInt *ci = dyn_cast<ConstantInt>(v))
			return vc_bv32ConstExprFromInt(vc, ci->getSExtValue());
		VCType int_type = vc_bv32Type(vc);
		ObjectID &OI = getAnalysis<ObjectID>();
		unsigned value_id = OI.getValueID(v);
		assert(value_id != ObjectID::INVALID_ID);
		ostringstream oss;
		oss << "x" << value_id;
		return vc_varExpr(vc, oss.str().c_str(), int_type);
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
		AU.addRequired<DominatorTree>();
		AU.addRequired<IntraReach>();
		AU.addRequired<CaptureConstraints>();
		ModulePass::getAnalysisUsage(AU);
	}

	bool SolveConstraints::satisfiable(
			const vector<const Clause *> &more_clauses) {
		vc_push(vc);
		forallconst(vector<const Clause *>, it, more_clauses)
			realize_uses(*it);
		forallconst(vector<const Clause *>, it, more_clauses) {
			const Clause *c = *it;
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
			realize_uses(*it);
		VCExpr conj = vc_trueExpr(vc);
		forallconst(vector<const Clause *>, it, more_clauses) {
			const Clause *c = *it;
			conj = vc_andExpr(vc, conj, translate_to_vc(c));
		}
		int ret = vc_query(vc, conj);
		vc_pop(vc);
		return ret == 1;
	}

	void SolveConstraints::realize_uses(const Clause *c) {
		if (c->be)
			realize_uses(c->be);
		else {
			realize_uses(c->c1);
			realize_uses(c->c2);
		}
	}

	void SolveConstraints::realize_uses(const BoolExpr *be) {
		realize_uses(be->e1);
		realize_uses(be->e2);
	}

	void SolveConstraints::realize_uses(const Expr *e) {
		if (e->type == Expr::Unary) {
			realize_uses(e->e1);
		} else if (e->type == Expr::Binary) {
			realize_uses(e->e1);
			realize_uses(e->e2);
		} else if (e->type == Expr::SingleUse) {
			realize_use(e->u);
		}
	}

	void SolveConstraints::realize_use(const Use *u) {
		const Instruction *ins = dyn_cast<Instruction>(u->getUser());
		// The value of a llvm::Constant is compile-time known. Therefore,
		// we don't need to capture extra constraints. 
		if (!ins)
			return;
		BasicBlock *bb = const_cast<BasicBlock *>(ins->getParent());
		Function *f = bb->getParent();
		DominatorTree &DT = getAnalysis<DominatorTree>(*f);
		IntraReach &IR = getAnalysis<IntraReach>(*f);
		while (bb != &f->getEntryBlock()) {
			DomTreeNode *node = DT[bb];
			BasicBlock *p = node->getIDom()->getBlock();
			assert(p);
			CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
			/* TODO: We only handle BranchInst with ICmpInst for now. */
			if (BranchInst *bi = dyn_cast<BranchInst>(p->getTerminator())) {
				ICmpInst *cond = dyn_cast<ICmpInst>(bi->getCondition());
				if (cond && CC.is_constant(cond)) {
					assert(bi->getNumSuccessors() == 2);
					/*
					 * -1: Neither leads to <bb> (Impossible)
					 * 0: Only successor 0 leads to <bb>
					 * 1: Only successor 1 leads to <bb>
					 * -2: Both lead to <bb>
					 */
					int leads_to_bb = -1;
					if (IR.reachable(bi->getSuccessor(0), bb))
						leads_to_bb = 0;
					if (IR.reachable(bi->getSuccessor(1), bb)) {
						if (leads_to_bb >= 0)
							leads_to_bb = -2;
						else
							leads_to_bb = 1;
					}
					assert(leads_to_bb != -1);
					if (leads_to_bb == 0 || leads_to_bb == 1) {
						CmpInst::Predicate pred = cond->getPredicate();
						const Clause *c = new Clause(new BoolExpr(
									leads_to_bb == 0 ? pred: CmpInst::getInversePredicate(pred),
									new Expr(cond->getOperand(0)),
									new Expr(cond->getOperand(1))));
						errs() << "new clause:";
						print_clause(errs(), c, getAnalysis<ObjectID>());
						errs() << "\n";
						VCExpr vce = translate_to_vc(c);
						vc_assertFormula(vc, vce);
						delete c;
					}
				}
			}
			bb = p;
		}
	}

	char SolveConstraints::ID = 0;
}
