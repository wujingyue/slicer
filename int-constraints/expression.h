/**
 * Author: Jingyue
 */

#ifndef __SLICER_EXPRESSION_H
#define __SLICER_EXPRESSION_H

#include "llvm/Instruction.h"
#include "llvm/Use.h"
#include "idm/id.h"
using namespace llvm;

#include <cstdio>
using namespace std;

namespace slicer {

	struct Expr {

		enum Type {
			SingleDef,
			SingleUse,
			Unary,
			Binary
		} type;
		unsigned op;
		Expr *e1, *e2;
		union {
			const Value *v;
			const Use *u;
		};

		Expr(const Use *use): u(use) {
			type = SingleUse;
			e1 = e2 = NULL;
		}
		
		Expr(const Value *value): v(value) {
			type = SingleDef;
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
			op(opcode), be(NULL), c1(lhs), c2(rhs)
		{
			assert(op == Instruction::And || op == Instruction::Or);
			assert(c1 != this && c2 != this && c1 != c2);
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

	void print_opcode(raw_ostream &O, unsigned op);
	void print_predicate(raw_ostream &O, CmpInst::Predicate p);
	void print_expr(raw_ostream &O, const Expr *e, ObjectID &OI);
	void print_bool_expr(raw_ostream &O, const BoolExpr *be, ObjectID &OI);
	void print_clause(raw_ostream &O, const Clause *c, ObjectID &OI);

	/*
	 * Sort the clauses according to the alphabetic order
	 */
	struct CompareClause {

		CompareClause(ObjectID &IDM): OI(IDM) {}
		
		bool operator()(const Clause *a, const Clause *b) {
			string str_a, str_b;
			raw_string_ostream oss_a(str_a), oss_b(str_b);
			print_clause(oss_a, a, OI);
			print_clause(oss_b, b, OI);
			return oss_a.str() < oss_b.str();
		}

	private:
		ObjectID &OI;
	};
}

#endif
