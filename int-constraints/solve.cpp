#include "common/include/util.h"
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
		vc = vc_createValidityChecker();
	}

	SolveConstraints::~SolveConstraints() {
		vc_Destroy(vc);
	}

	bool SolveConstraints::runOnModule(Module &M) {
		CaptureConstraints &CC = getAnalysis<CaptureConstraints>();
		for (unsigned i = 0; i < CC.get_num_constraints(); ++i) {
			const Clause *c = CC.get_constraint(i);
			VCExpr vc_expr = translate_to_vc(c);
			vc_assertFormula(vc, vc_expr);
		}
		return false;
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
		if (e->type == Expr::SingleValue)
			return translate_to_vc(e->v);
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
					return vc_bvDivExpr(vc, 32, left, right);
				case Instruction::URem:
				case Instruction::SRem:
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

	void SolveConstraints::print(raw_ostream &O, const Module *M) const {
		// Don't know what to do. 
	}

	void SolveConstraints::getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
		AU.addRequiredTransitive<ObjectID>();
		AU.addRequired<CaptureConstraints>();
		ModulePass::getAnalysisUsage(AU);
	}

	bool SolveConstraints::satisfiable(
			const vector<const Clause *> &more_clauses) {
		vc_push(vc);
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
		VCExpr conj = vc_trueExpr(vc);
		forallconst(vector<const Clause *>, it, more_clauses) {
			const Clause *c = *it;
			conj = vc_andExpr(vc, conj, translate_to_vc(c));
		}
		int ret = vc_query(vc, conj);
		vc_pop(vc);
		return ret == 1;
	}

	char SolveConstraints::ID = 0;
}
