/**
 * Author: Jingyue
 */

#define DEBUG_TYPE "int"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Analysis/Dominators.h"
#include "common/include/util.h"
#include "common/cfg/intra-reach.h"
using namespace llvm;

#include <list>
#include <iostream>
#include <sstream>
using namespace std;

#include "capture.h"
#include "solve.h"
#include "config.h"
using namespace slicer;

VC SolveConstraints::vc = NULL;
sys::Mutex SolveConstraints::vc_mutex(false); // not recursive
DenseMap<string, VCExpr> SolveConstraints::symbols;

int SolveConstraints::try_to_simplify(VCExpr e) {
	vc_push(vc);
	VCExpr simplified = vc_simplify(vc, e);
	int ret = vc_isBool(simplified);
	delete_vcexpr(simplified);
	vc_pop(vc);
	return ret;
}

void SolveConstraints::destroy_vc() {
	assert(vc && "create_vc and destroy_vc are not paired");
	for (DenseMap<string, VCExpr>::iterator it = symbols.begin();
			it != symbols.end(); ++it)
		vc_DeleteExpr(it->second);
	symbols.clear();
	vc_Destroy(vc);
	vc = NULL;
	vc_mutex.release();
}

void SolveConstraints::create_vc() {
	assert(vc_mutex.tryacquire() && "There can be only one VC instance running");
	assert(!vc && "create_vc and destroy_vc are not paired");
	vc = vc_createValidityChecker();
	// Don't delete persistant nodes on vc_Destroy. 
	// We are responsible to delete them. 
	vc_setInterfaceFlags(vc, EXPRDELETE, 0);
	vc_registerErrorHandler(vc_error_handler);
	assert(symbols.empty());
	assert(vc && "Failed to create a VC");
}

void SolveConstraints::vc_error_handler(const char *err_msg) {
	errs() << "Error in VC: ";
	errs() << err_msg << "\n";
}

VCExpr SolveConstraints::translate_to_vc(const Clause *c) {
	if (c->be)
		return translate_to_vc(c->be);
	VCExpr vce1 = translate_to_vc(c->c1);
	VCExpr vce2 = (c->c2 == NULL ? NULL : translate_to_vc(c->c2));
	VCExpr res;
	if (c->op == Instruction::And)
		res = vc_andExpr(vc, vce1, vce2);
	else if (c->op == Instruction::Or)
		res = vc_orExpr(vc, vce1, vce2);
	else if (c->op == Instruction::Xor)
		res = vc_xorExpr(vc, vce1, vce2);
	else {
		assert(c->op == Instruction::UserOp1);
		res = vc_notExpr(vc, vce1);
	}
	delete_vcexpr(vce1);
	if (vce2)
		delete_vcexpr(vce2);
	return res;
}

VCExpr SolveConstraints::translate_to_vc(const BoolExpr *be) {
	const Expr *e1 = be->e1, *e2 = be->e2;
	assert(e1->get_width() == e2->get_width());
	VCExpr vce1 = translate_to_vc(e1);
	VCExpr vce2 = translate_to_vc(e2);
	VCExpr res;
	switch (be->p) {
		case CmpInst::ICMP_EQ:
			res = vc_eqExpr(vc, vce1, vce2);
			break;
		case CmpInst::ICMP_NE:
			{
				VCExpr eq = vc_eqExpr(vc, vce1, vce2);
				res = vc_notExpr(vc, eq);
				delete_vcexpr(eq);
			}
			break;
		case CmpInst::ICMP_UGT:
			res = vc_bvGtExpr(vc, vce1, vce2);
			break;
		case CmpInst::ICMP_SGT:
			res = vc_sbvGtExpr(vc, vce1, vce2);
			break;
		case CmpInst::ICMP_UGE:
			res = vc_bvGeExpr(vc, vce1, vce2);
			break;
		case CmpInst::ICMP_SGE:
			res = vc_sbvGeExpr(vc, vce1, vce2);
			break;
		case CmpInst::ICMP_ULT:
			res = vc_bvLtExpr(vc, vce1, vce2);
			break;
		case CmpInst::ICMP_SLT:
			res = vc_sbvLtExpr(vc, vce1, vce2);
			break;
		case CmpInst::ICMP_ULE:
			res = vc_bvLeExpr(vc, vce1, vce2);
			break;
		case CmpInst::ICMP_SLE:
			res = vc_sbvLeExpr(vc, vce1, vce2);
			break;
		default: assert(false && "Invalid predicate");
	}
	delete_vcexpr(vce1);
	delete_vcexpr(vce2);
	return res;
}

VCExpr SolveConstraints::translate_to_vc(const Expr *e) {
	if (e->type == Expr::SingleDef)
		return translate_to_vc(e->v, e->context);
	if (e->type == Expr::LoopBound)
		return translate_to_vc(e->v, e->context, true);
	if (e->type == Expr::SingleUse)
		return translate_to_vc(e->u, e->context);
	if (e->type == Expr::Unary) {
		VCExpr child = translate_to_vc(e->e1);
		VCExpr res;
		switch (e->op) {
			case Instruction::SExt:
				assert(e->e1->get_width() == 1);
				res = vc_bvSignExtend(vc, child, 32);
				break;
			case Instruction::ZExt:
				{
					assert(e->e1->get_width() == 1);
					// STP does not have bvUnsignExtend
					VCExpr zero_31 = vc_bvConstExprFromInt(vc, 31, 0);
					res = vc_bvConcatExpr(vc, zero_31, child);
					delete_vcexpr(zero_31);
				}
				break;
			case Instruction::Trunc:
				assert(e->e1->get_width() == 32);
				res = vc_bvExtract(vc, child, 0, 0);
				break;
			default: assert_not_supported();
		}
		delete_vcexpr(child);
		return res;
	}
	if (e->type == Expr::Binary) {
		VCExpr left = translate_to_vc(e->e1);
		VCExpr right = translate_to_vc(e->e2);
		avoid_overflow(e->op, left, right);
		VCExpr res;
		switch (e->op) {
			case Instruction::Add:
				res = vc_bv32PlusExpr(vc, left, right);
				break;
			case Instruction::Sub:
				res = vc_bv32MinusExpr(vc, left, right);
				break;
			case Instruction::Mul:
				res = vc_bv32MultExpr(vc, left, right);
				break;
			case Instruction::UDiv:
			case Instruction::SDiv:
				res = vc_sbvDivExpr(vc, 32, left, right);
				break;
			case Instruction::URem:
			case Instruction::SRem:
				res = vc_sbvModExpr(vc, 32, left, right);
				break;
			case Instruction::Shl:
				// left << right
				if (getExprKind(right) == BVCONST)
					res = vc_bv32LeftShiftExpr(vc, getBVUnsigned(right), left);
				else
					res = vc_bvVar32LeftShiftExpr(vc, right, left);
				break;
			case Instruction::LShr:
			case Instruction::AShr:
				// left >> right
				if (getExprKind(right) == BVCONST)
					res = vc_bv32RightShiftExpr(vc, getBVUnsigned(right), left);
				else
					res = vc_bvVar32RightShiftExpr(vc, right, left);
				break;
			case Instruction::And:
				res = vc_bvAndExpr(vc, left, right);
				break;
			case Instruction::Or:
				res = vc_bvOrExpr(vc, left, right);
				break;
			case Instruction::Xor:
				res = vc_bvXorExpr(vc, left, right);
				break;
			default: assert_not_supported();
		}
		delete_vcexpr(left);
		delete_vcexpr(right);
		return res;
	}
	assert(false && "Invalid expression type");
}

VCExpr SolveConstraints::translate_to_vc(const Value *v,
		unsigned context, bool is_loop_bound) {
	if (const ConstantInt *ci = dyn_cast<ConstantInt>(v)) {
		if (ci->getType()->getBitWidth() == 1) {
			VCExpr b = (ci->isOne() ? vc_trueExpr(vc) : vc_falseExpr(vc));
			VCExpr res = vc_boolToBVExpr(vc, b);
			delete_vcexpr(b);
			return res;
		} else {
			// TODO: Add warnings on very large constants.
			return vc_bv32ConstExprFromInt(vc, ci->getSExtValue());
		}
	}
	if (isa<ConstantPointerNull>(v)) {
		// null == 0
		return vc_zero(vc);
	}

	IDAssigner &IDA = getAnalysis<IDAssigner>();
	unsigned value_id = IDA.getValueID(v);
	assert(value_id != IDAssigner::INVALID_ID);

	ostringstream oss;
	oss << (is_loop_bound ? "lb": "x") << value_id;
	if (context != 0)
		oss << "_" << context;

	string name = oss.str();
	VCExpr &symbol = symbols[name];
	if (symbol == NULL) {
		VCType vct = (v->getType()->isIntegerTy(1) ?
				vc_bvType(vc, 1) :
				vc_bv32Type(vc));
		symbol = vc_varExpr(vc, name.c_str(), vct);
		delete_vcexpr(vct);
	}

	return symbol;
}

VCExpr SolveConstraints::translate_to_vc(const Use *u, unsigned context) {
	return translate_to_vc(u->get(), context);
}

void SolveConstraints::avoid_div_by_zero(VCExpr left, VCExpr right) {

	// TODO: We shouldn't assume the divisor > 0
	VCExpr zero = vc_zero(vc);
	VCExpr right_gt_0 = vc_sbvGtExpr(vc, right, zero);

	vc_assertFormula(vc, right_gt_0);

	delete_vcexpr(zero);
	delete_vcexpr(right_gt_0);
}

void SolveConstraints::avoid_overflow_shl(VCExpr left, VCExpr right) {

	// (left << right) <= oo ==> left <= (oo >> right)
	int bit_width = vc_getBVLength(vc, left);
	assert(vc_getBVLength(vc, right) == bit_width);

	VCExpr int_max = vc_int_max(vc);
	VCExpr left_ge_0 = vc_bvBoolExtract_Zero(vc, left, bit_width - 1);
	VCExpr int_max_shr = vc_bvVar32RightShiftExpr(vc, right, int_max);
	VCExpr left_le = vc_sbvLeExpr(vc, left, int_max_shr);

	vc_assertFormula(vc, left_ge_0);
	vc_assertFormula(vc, left_le);

	delete_vcexpr(int_max);
	delete_vcexpr(left_ge_0);
	delete_vcexpr(int_max_shr);
	delete_vcexpr(left_le);
}

void SolveConstraints::avoid_overflow_sub(VCExpr left, VCExpr right) {
	// -oo <= left + (-right) <= oo
	VCExpr minus_right = vc_bvUMinusExpr(vc, right);
	avoid_overflow_add(left, minus_right);
	delete_vcexpr(minus_right);
}

void SolveConstraints::avoid_overflow_add(VCExpr left, VCExpr right) {
	
	// -oo <= left + right <= oo
	int bit_width = vc_getBVLength(vc, left);
	assert(vc_getBVLength(vc, right) == bit_width);

	VCExpr sum = vc_bv32PlusExpr(vc, left, right);
	VCExpr h_left = vc_bvBoolExtract_One(vc, left, bit_width - 1);
	VCExpr h_right = vc_bvBoolExtract_One(vc, right, bit_width - 1);
	VCExpr h_sum = vc_bvBoolExtract_One(vc, sum, bit_width - 1);
	VCExpr xor_expr = vc_xorExpr(vc, h_left, h_right);
	VCExpr iff_expr = vc_iffExpr(vc, h_right, h_sum);
	VCExpr or_expr = vc_orExpr(vc, xor_expr, iff_expr);

	vc_assertFormula(vc, or_expr);
	
	delete_vcexpr(sum);
	delete_vcexpr(h_left);
	delete_vcexpr(h_right);
	delete_vcexpr(h_sum);
	delete_vcexpr(xor_expr);
	delete_vcexpr(iff_expr);
	delete_vcexpr(or_expr);
}

void SolveConstraints::avoid_overflow_mul(VCExpr left, VCExpr right) {
#if 0
	// TODO: does not support negative numbers
	/*
	 * FIXME: impliesExpr doesn't work as expected. 
	 * We expected to use it to get around the "div-by-zero" problem.
	 * Therefore, we extend the operands to 64-bit integers, and check
	 * whether the product is really out of range. 
	 */
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
#endif
#if 0
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
}

void SolveConstraints::avoid_overflow(unsigned op, VCExpr left, VCExpr right) {
	switch (op) {
		case Instruction::Add:
#ifdef CHECK_BOUND
			avoid_overflow_add(left, right);
#endif
			break;
		case Instruction::Sub:
#ifdef CHECK_BOUND
			avoid_overflow_sub(left, right);
#endif
			break;
		case Instruction::Mul:
#ifdef CHECK_BOUND
			avoid_overflow_mul(left, right);
#endif
			break;
		case Instruction::UDiv:
		case Instruction::SDiv:
		case Instruction::URem:
		case Instruction::SRem:
#ifdef CHECK_DIV
			avoid_div_by_zero(left, right);
#endif
			break;
		case Instruction::Shl:
#ifdef CHECK_BOUND
			avoid_overflow_shl(left, right);
#endif
			break;
	}
}

void SolveConstraints::delete_vcexpr(VCExpr e) {
	if (getExprKind(e) == SYMBOL)
		return;
	vc_DeleteExpr(e);
}
