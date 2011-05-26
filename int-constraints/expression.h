#ifndef __SLICER_EXPRESSION_H
#define __SLICER_EXPRESSION_H

#include <string>
using namespace std;

namespace slicer {

	inline void print_opcode(raw_ostream &O, unsigned op) {
		switch (op) {
			case Instruction::Add: O << "+"; break;
			case Instruction::Sub: O << "-"; break;
			case Instruction::Mul: O << "*"; break;
			case Instruction::UDiv:
			case Instruction::SDiv: O << "/"; break;
			case Instruction::URem:
			case Instruction::SRem: O << "%"; break;
			case Instruction::Shl: O << "<<"; break;
			case Instruction::LShr:
			case Instruction::AShr: O << ">>"; break; // FIXME: distinguish them
			case Instruction::And: O << "&"; break;
			case Instruction::Or: O << "|"; break;
			case Instruction::Xor: O << "^"; break;
			default: assert(false && "Not supported");
		}
	}

	inline void print_predicate(raw_ostream &O, CmpInst::Predicate p) {
		switch (p) {
			case CmpInst::ICMP_EQ: O << "="; break;
			case CmpInst::ICMP_NE: O << "!="; break;
			case CmpInst::ICMP_UGT:
			case CmpInst::ICMP_SGT: O << ">"; break;
			case CmpInst::ICMP_UGE:
			case CmpInst::ICMP_SGE: O << ">="; break;
			case CmpInst::ICMP_ULT:
			case CmpInst::ICMP_SLT: O << "<"; break;
			case CmpInst::ICMP_ULE:
			case CmpInst::ICMP_SLE: O << "<="; break;
			default: assert(false && "Not supported");
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

		Expr(unsigned opcode, Expr *expr): op(opcode), e1(expr) {
			type = Unary;
			v = NULL;
			e2 = NULL;
		}

		Expr(unsigned opcode, Expr *expr1, Expr *expr2) {
			type = Binary;
			op = opcode;
			e1 = expr1;
			e2 = expr2;
			v = NULL;
		}

		virtual ~Expr() {
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
			p(pred), e1(expr1), e2(expr2) {}

		~BoolExpr() {
			delete e1;
			delete e2;
			e1 = e2 = NULL;
		}
	};

	// A clause is a set of boolean expressions connected with AND or OR. 
	struct Clause {

		enum Op {
			None,
			And,
			Or
		} op;

		BoolExpr *be;
		Clause *c1, *c2;

		static Clause *create_and(Clause *lhs, Clause *rhs) {
			Clause *c = new Clause();
			c->op = And;
			c->be = NULL;
			c->c1 = lhs;
			c->c2 = rhs;
			return c;
		}

		static Clause *create_or(Clause *lhs, Clause *rhs) {
			Clause *c = new Clause();
			c->op = Or;
			c->be = NULL;
			c->c1 = lhs;
			c->c2 = rhs;
			return c;
		}

		static Clause *create_bool_expr(BoolExpr *expr) {
			Clause *c = new Clause();
			c->op = None;
			c->be = expr;
			c->c1 = c->c2 = NULL;
			return c;
		}

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
