#include "idm/id.h"
using namespace llvm;

#include "expression.h"
#include "capture.h"

namespace slicer {

	void CaptureConstraints::print_expr(raw_ostream &O, const Expr *e) const {
		if (e->type == Expr::SingleValue) {
			ObjectID &OI = getAnalysis<ObjectID>();
			unsigned value_id = OI.getValueID(e->v);
			assert(value_id != ObjectID::INVALID_ID);
			O << value_id;
		} else if (e->type == Expr::Unary) {
			O << "(";
			print_opcode(O, e->op);
			O << " ";
			print_expr(O, e->e1);
			O << ")";
		} else if (e->type == Expr::Binary) {
			O << "(";
			print_expr(O, e->e1);
			O << " ";
			print_opcode(O, e->op);
			O << " ";
			print_expr(O, e->e2);
			O << ")";
		} else
			assert(false && "Unknown type");
	}

	void CaptureConstraints::print_bool_expr(
			raw_ostream &O, const BoolExpr *be) const {
		O << "(";
		print_expr(O, be->e1);
		O << " ";
		print_predicate(O, be->p);
		O << " ";
		print_expr(O, be->e2);
		O << ")";
	}

	void CaptureConstraints::print_clause(raw_ostream &O, const Clause *c) const {
		if (c->op == Clause::None) {
			print_bool_expr(O, c->be);
			return;
		}
		O << "(";
		print_clause(O, c->c1);
		O << (c->op == Clause::And ? " AND " : " OR ");
		print_clause(O, c->c2);
		O << ")";
	}
}
