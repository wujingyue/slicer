/**
 * Author: Jingyue
 */

#ifndef __SLICER_EXPRESSION_H
#define __SLICER_EXPRESSION_H

#include "llvm/Instruction.h"
#include "llvm/Use.h"
#include "common/IDAssigner.h"
#include "common/typedefs.h"
using namespace llvm;

#include <cstdio>
using namespace std;

namespace slicer {
	/**
	 * We don't allow any Expr, BoolExpr or Clause has more than one references. 
	 * TODO: We could allow that by adding reference counting. 
	 */
	struct Expr {
		enum Type {
			SingleDef, SingleUse, LoopBound,
			Unary, Binary
		} type;
		unsigned op, context;
		Expr *e1, *e2;
		union {
			const Value *v;
			const Use *u;
		};
		InstList callstack;

		Expr *clone() const;
		unsigned get_width() const;
		Expr(const Use *use, unsigned c = 0);
		// <t> can be LoopBound as well, although seldom used. 
		// FIXME: looks quite ugly. 
		Expr(const Value *value, unsigned c = 0, enum Type t = SingleDef);
		Expr(unsigned opcode, Expr *expr);
		Expr(unsigned opcode, Expr *expr1, Expr *expr2);
		~Expr();
	};

	// Expressions connected with predicates. 
	struct BoolExpr {
		CmpInst::Predicate p;
		Expr *e1, *e2;

		BoolExpr *clone() const;
		BoolExpr(CmpInst::Predicate pred, Expr *expr1, Expr *expr2);
		~BoolExpr();
	};

	// A clause is a set of boolean expressions connected with AND, OR, or XOR
	struct Clause {
		unsigned op;
		BoolExpr *be;
		Clause *c1, *c2;

		Clause *clone() const;
		Clause(unsigned opcode, Clause *lhs, Clause *rhs);
		Clause(unsigned opcode, Clause *child);
		Clause(BoolExpr *expr);
		~Clause();
	};

	void print_opcode(raw_ostream &O, unsigned op);
	void print_predicate(raw_ostream &O, CmpInst::Predicate p);
	void print_expr(raw_ostream &O, const Expr *e, IDAssigner &IDA);
	void print_bool_expr(raw_ostream &O, const BoolExpr *be, IDAssigner &IDA);
	void print_clause(raw_ostream &O, const Clause *c, IDAssigner &IDA);

	/*
	 * Sort the clauses according to the alphabetic order
	 */
	struct CompareClause {
		CompareClause(IDAssigner &ida): IDA(ida) {}
		bool operator()(const Clause *a, const Clause *b);
	private:
		IDAssigner &IDA;
	};

	class ClauseVisitor {
		virtual void visit_clause(Clause *c);
		virtual void visit_bool_expr(BoolExpr *be);
		virtual void visit_expr(Expr *e);
	public:
		virtual void visit_single_def(const Value *v);
		virtual void visit_single_use(const Use *u);
		virtual void visit_loop_bound(const Value *v);
		friend struct Clause;
	};
}

#endif
