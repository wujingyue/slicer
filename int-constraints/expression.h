#ifndef __SLICER_EXPRESSION_H
#define __SLICER_EXPRESSION_H

#include "llvm/Instruction.h"
using namespace llvm;

namespace slicer {

	inline void print_opcode(raw_ostream &O, unsigned op) {
		switch (op) {
			case Instruction::Add:
				O << "+";
				break;
			case Instruction::Sub:
				O << "-";
				break;
			case Instruction::Mul:
				O << "*";
				break;
			case Instruction::UDiv:
			case Instruction::SDiv:
				O << "/";
				break;
			case Instruction::URem:
			case Instruction::SRem:
				O << "%";
				break;
			case Instruction::Shl:
				O << "<<";
				break;
			case Instruction::LShr:
			case Instruction::AShr:
				O << ">>";
				break; // FIXME: distinguish them
			case Instruction::And:
				O << "&";
				break;
			case Instruction::Or:
				O << "|";
				break;
			case Instruction::Xor:
				O << "^";
				break;
			default: assert_not_supported();
		}
	}

	inline void print_predicate(raw_ostream &O, CmpInst::Predicate p) {
		switch (p) {
			case CmpInst::ICMP_EQ:
				O << "=";
				break;
			case CmpInst::ICMP_NE:
				O << "!=";
				break;
			case CmpInst::ICMP_UGT:
			case CmpInst::ICMP_SGT:
				O << ">";
				break;
			case CmpInst::ICMP_UGE:
			case CmpInst::ICMP_SGE:
				O << ">=";
				break;
			case CmpInst::ICMP_ULT:
			case CmpInst::ICMP_SLT:
				O << "<";
				break;
			case CmpInst::ICMP_ULE:
			case CmpInst::ICMP_SLE:
				O << "<=";
				break;
			default: assert(false && "Invalid predicate");
		}
	}

	struct Expr {

		enum Type {
			SingleValue,
			Unary,
			Binary
		} type;
		unsigned op;
		Expr *e1, *e2;
		const Value *v;

		Expr(const Value *val): v(val) {
			type = SingleValue;
			e1 = e2 = NULL;
		}

		Expr(unsigned opcode, Expr *expr):
			type(Unary), op(opcode), e1(expr), e2(NULL), v(NULL)
		{
			assert_not_supported();
		}

		Expr(unsigned opcode, Expr *expr1, Expr *expr2):
			type(Binary), op(opcode), e1(expr1), e2(expr2), v(NULL)
		{
			switch (opcode) {
				case Instruction::Add:
				case Instruction::Sub:
				case Instruction::Mul:
				case Instruction::UDiv:
				case Instruction::SDiv:
				case Instruction::URem:
				case Instruction::SRem:
				case Instruction::Shl:
				case Instruction::LShr:
				case Instruction::AShr:
				case Instruction::And:
				case Instruction::Or:
				case Instruction::Xor:
					break;
				default: assert_not_supported();
			}
		}

		~Expr() {
			if (e1) {
				delete e1;
				e1 = NULL;
			}
			if (e2) {
				delete e2;
				e2 = NULL;
			}
		}
	};

	// Expressions connected with predicates. 
	struct BoolExpr {

		CmpInst::Predicate p;
		Expr *e1, *e2;

		BoolExpr(CmpInst::Predicate pred, Expr *expr1, Expr *expr2):
			p(pred), e1(expr1), e2(expr2)
		{
			switch (p) {
				case CmpInst::ICMP_EQ:
				case CmpInst::ICMP_NE:
				case CmpInst::ICMP_UGT:
				case CmpInst::ICMP_UGE:
				case CmpInst::ICMP_ULT:
				case CmpInst::ICMP_ULE:
				case CmpInst::ICMP_SGT:
				case CmpInst::ICMP_SGE:
				case CmpInst::ICMP_SLT:
				case CmpInst::ICMP_SLE:
					break;
				default: assert(false && "Invalid predicate");
			}
		}

		~BoolExpr() {
			delete e1;
			delete e2;
			e1 = e2 = NULL;
		}
	};

	// A clause is a set of boolean expressions connected with AND or OR. 
	struct Clause {

		unsigned op;
		BoolExpr *be;
		Clause *c1, *c2;

		Clause(unsigned opcode, Clause *lhs, Clause *rhs):
			op(opcode), be(NULL), c1(lhs), c2(lhs)
		{
			assert(op == Instruction::And || op == Instruction::Or);
		}

		Clause(BoolExpr *expr): op(0), be(expr), c1(NULL), c2(NULL) {}

		~Clause() {
			if (c1) {
				delete c1;
				c1 = NULL;
			}
			if (c2) {
				delete c2;
				c2 = NULL;
			}
		}
	};
}

#endif
