#include "common/include/util.h"
using namespace llvm;

#include "expression.h"
#include "capture.h"
using namespace slicer;

bool CompareClause::operator()(const Clause *a, const Clause *b) {
	
	string str_a, str_b;
	raw_string_ostream oss_a(str_a), oss_b(str_b);
	print_clause(oss_a, a, IDA);
	print_clause(oss_b, b, IDA);
	oss_a.flush();
	oss_b.flush();

	unsigned n_brackets_a = 0, n_brackets_b = 0;
	for (size_t i = 0; i < str_a.length(); ++i)
		n_brackets_a += (str_a[i] == '(' || str_a[i] == ')');
	for (size_t i = 0; i < str_b.length(); ++i)
		n_brackets_b += (str_b[i] == '(' || str_b[i] == ')');
	// Put simple constraints up front. 
	return n_brackets_a < n_brackets_b ||
		(n_brackets_a == n_brackets_b && str_a < str_b);
}

Expr *Expr::clone() const {
	if (type == SingleDef || type == LoopBound)
		return new Expr(v, context, type);
	if (type == SingleUse)
		return new Expr(u, context);
	if (type == Unary)
		return new Expr(op, e1->clone());
	if (type == Binary)
		return new Expr(op, e1->clone(), e2->clone());
	assert_unreachable();
}

Expr::~Expr() {
	if (e1) {
		delete e1;
		e1 = NULL;
	}
	if (e2) {
		delete e2;
		e2 = NULL;
	}
}

unsigned Expr::get_width() const {
	if (type == SingleDef || type == LoopBound || type == SingleUse) {
		const Value *val = (type == SingleUse ? u->get(): v);
		return (val->getType()->isIntegerTy(1) ? 1 : 32);
	}
	if (type == Unary) {
		if (op == Instruction::Trunc)
			return 1;
		else
			return 32;
	}
	if (type == Binary) {
		assert(e1->get_width() == e2->get_width());
		return e1->get_width();
	}
	assert_unreachable();
}

Expr::Expr(const Use *use, unsigned c) {
	type = SingleUse;
	e1 = e2 = NULL;
	context = c;
	u = use;
}

// <t> can be LoopBound as well, although seldom used. 
Expr::Expr(const Value *value, unsigned c, enum Type t) {
	type = t;
	e1 = e2 = NULL;
	v = value;
	context = c;
}

Expr::Expr(unsigned opcode, Expr *expr) {
	type = Unary;
	op = opcode;
	e1 = expr;
	e2 = NULL;
	v = NULL;
	assert(opcode == Instruction::ZExt || opcode == Instruction::SExt ||
			opcode == Instruction::Trunc);
}

Expr::Expr(unsigned opcode, Expr *expr1, Expr *expr2) {
	type = Binary;
	op = opcode;
	e1 = expr1;
	e2 = expr2;
	v = NULL;
	assert(e1->get_width() == e2->get_width());
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

BoolExpr *BoolExpr::clone() const {
	return new BoolExpr(p, e1->clone(), e2->clone());
}

BoolExpr::BoolExpr(CmpInst::Predicate pred, Expr *expr1, Expr *expr2) {
	p = pred;
	e1 = expr1;
	e2 = expr2;
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

BoolExpr::~BoolExpr() {
	delete e1;
	delete e2;
	e1 = e2 = NULL;
}

Clause *Clause::clone() const {
	if (be)
		return new Clause(be->clone());
	else if (op == Instruction::UserOp1)
		return new Clause(op, c1->clone());
	else
		return new Clause(op, c1->clone(), c2->clone());
}

Clause::Clause(unsigned opcode, Clause *lhs, Clause *rhs) {
	op = opcode;
	be = NULL;
	c1 = lhs;
	c2 = rhs;
	assert(op == Instruction::And || op == Instruction::Or ||
			op == Instruction::Xor);
	assert(c1 != this && c2 != this && c1 != c2);
}

Clause::Clause(unsigned opcode, Clause *child) {
	op = opcode;
	be = NULL;
	c1 = child;
	c2 = NULL;
	assert(op == Instruction::UserOp1);
	assert(c1 != this);
}

Clause::Clause(BoolExpr *expr) {
	op = 0;
	be = expr;
	c1 = c2 = NULL;
}

Clause::~Clause() {
	if (c1) {
		delete c1;
		c1 = NULL;
	}
	if (c2) {
		delete c2;
		c2 = NULL;
	}
}

void slicer::print_opcode(raw_ostream &O, unsigned op) {
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
		case Instruction::ZExt:
		case Instruction::SExt:
			O << "<";
			break;
		case Instruction::Trunc:
			O << ">";
		default: assert_not_supported();
	}
}

void slicer::print_predicate(raw_ostream &O, CmpInst::Predicate p) {
	switch (p) {
		case CmpInst::ICMP_EQ:
			O << "=";
			break;
		case CmpInst::ICMP_NE:
			O << "!=";
			break;
		case CmpInst::ICMP_UGT:
			O << "u>";
			break;
		case CmpInst::ICMP_SGT:
			O << ">";
			break;
		case CmpInst::ICMP_UGE:
			O << "u>=";
			break;
		case CmpInst::ICMP_SGE:
			O << ">=";
			break;
		case CmpInst::ICMP_ULT:
			O << "u<";
			break;
		case CmpInst::ICMP_SLT:
			O << "<";
			break;
		case CmpInst::ICMP_ULE:
			O << "u<=";
			break;
		case CmpInst::ICMP_SLE:
			O << "<=";
			break;
		default: assert(false && "Invalid predicate");
	}
}

void slicer::print_expr(raw_ostream &O, const Expr *e, IDAssigner &IDA) {
	if (e->type == Expr::SingleDef || e->type == Expr::LoopBound ||
			e->type == Expr::SingleUse) {
		const Value *v = (e->type != Expr::SingleUse ? e->v : e->u->get());
		// Some ConstantInts are generated by our constraint capturer. 
		// They don't appear in the original module. 
		if (const ConstantInt *ci = dyn_cast<ConstantInt>(v)) {
			if (ci->getType()->getBitWidth() == 1)
				O << ci->getZExtValue();
			else
				O << ci->getSExtValue();
		} else if (isa<ConstantPointerNull>(v)) {
			O << "0";
		} else {
			unsigned value_id = IDA.getValueID(v);
			assert(value_id != IDAssigner::INVALID_ID);
			if (e->type == Expr::SingleDef)
				O << "x";
			else if (e->type == Expr::SingleUse)
				O << "u";
			else if (e->type == Expr::LoopBound)
				O << "lb";
			else
				assert_unreachable();
			O << value_id;
			if (e->context != 0)
				O << "_" << e->context;
		}
	} else if (e->type == Expr::Unary) {
		O << "(";
		print_opcode(O, e->op);
		O << " ";
		print_expr(O, e->e1, IDA);
		O << ")";
	} else if (e->type == Expr::Binary) {
		O << "(";
		print_expr(O, e->e1, IDA);
		O << " ";
		print_opcode(O, e->op);
		O << " ";
		print_expr(O, e->e2, IDA);
		O << ")";
	} else
		assert(false && "Unknown type");
}

void slicer::print_bool_expr(raw_ostream &O, const BoolExpr *be, IDAssigner &IDA) {
	O << "(";
	print_expr(O, be->e1, IDA);
	O << " ";
	print_predicate(O, be->p);
	O << " ";
	print_expr(O, be->e2, IDA);
	O << ")";
}

void slicer::print_clause(raw_ostream &O, const Clause *c, IDAssigner &IDA) {
	if (c->be) {
		print_bool_expr(O, c->be, IDA);
		return;
	}
	if (c->op == Instruction::UserOp1) {
		O << "(NOT ";
		print_clause(O, c->c1, IDA);
		O << ")";
		return;
	}
	O << "(";
	print_clause(O, c->c1, IDA);
	if (c->op == Instruction::And)
		O << " AND ";
	else if (c->op == Instruction::Or)
		O << " OR ";
	else
		O << " XOR ";
	print_clause(O, c->c2, IDA);
	O << ")";
}

void ClauseVisitor::visit_clause(Clause *c) {
	if (c->be)
		visit_bool_expr(c->be);
	else if (c->op == Instruction::UserOp1)
		visit_clause(c->c1);
	else {
		visit_clause(c->c1);
		visit_clause(c->c2);
	}
}

void ClauseVisitor::visit_bool_expr(BoolExpr *be) {
	visit_expr(be->e1);
	visit_expr(be->e2);
}

void ClauseVisitor::visit_expr(Expr *e) {
	if (e->type == Expr::SingleDef) {
		visit_single_def(e->v);
	} else if (e->type == Expr::LoopBound) {
		visit_loop_bound(e->v);
	} else if (e->type == Expr::SingleUse) {
		visit_single_use(e->u);
	} else if (e->type == Expr::Unary) {
		visit_expr(e->e1);
	} else if (e->type == Expr::Binary) {
		visit_expr(e->e1);
		visit_expr(e->e2);
	} else {
		assert_not_supported();
	}
}

void ClauseVisitor::visit_single_use(const Use *u) {
	// Do nothing by default. 
}

void ClauseVisitor::visit_single_def(const Value *v) {
	// Do nothing by default. 
}

void ClauseVisitor::visit_loop_bound(const Value *v) {
	// Do nothing by default. 
}
