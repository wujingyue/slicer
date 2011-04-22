#ifndef __SLICER_EXPRESSION_H
#define __SLICER_EXPRESSION_H

#include <string>
using namespace std;

namespace slicer {

	// Will be more structured. 
	typedef string Expr;

	// Expressions connected with predicates. 
	// Currently, the predicate must be <=
	struct BoolExpr {

		Expr e1, e2;

		BoolExpr() {}
		BoolExpr(const Expr expr1, const Expr expr2): e1(expr1), e2(expr2) {}
	};
	
	// A clause is a set of boolean expressions connected with AND or OR. 
	struct Clause {

		enum Op {
			None,
			And,
			Or
		} op;

		BoolExpr be;
		Clause *c1, *c2;

		static Clause *create_and(Clause *lhs, Clause *rhs) {
			Clause *c = new Clause();
			c->op = And;
			c->c1 = lhs;
			c->c2 = rhs;
			return c;
		}

		static Clause *create_or(Clause *lhs, Clause *rhs) {
			Clause *c = new Clause();
			c->op = Or;
			c->c1 = lhs;
			c->c2 = rhs;
			return c;
		}

		static Clause *create_bool_expr(const BoolExpr &expr) {
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

