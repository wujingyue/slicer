/**
 * Author: Jingyue
 *
 * Handle top-level variables. 
 */

#define DEBUG_TYPE "int"

#include "llvm/Support/Debug.h"
#include "llvm/Target/TargetData.h"
#include "common/callgraph-fp/callgraph-fp.h"
#include "common/cfg/exec-once.h"
#include "common/include/util.h"
using namespace llvm;

#include "capture.h"
using namespace slicer;

void CaptureConstraints::extract_from_consts(Constant *c) {
	// We don't care about functions. 
	// We might unlikely in the future, when we want to reason about integer
	// constraints on function pointers. 
	if (isa<Function>(c))
		return;
	// Only handle integer constants and pointer constants. 
	if (!isa<IntegerType>(c->getType()) && !isa<PointerType>(c->getType()))
		return;
	integers.insert(c);
	for (unsigned i = 0; i < c->getNumOperands(); ++i) {
		if (Constant *ct = dyn_cast<Constant>(c->getOperand(i)))
			extract_from_consts(ct);
	}
}

void CaptureConstraints::identify_integers(Module &M) {

	ExecOnce &EO = getAnalysis<ExecOnce>();
	
	integers.clear();
	// Global variables. 
	for (Module::global_iterator gi = M.global_begin();
			gi != M.global_end(); ++gi) {
		if (isa<IntegerType>(gi->getType()) || isa<PointerType>(gi->getType())) {
			integers.insert(gi);
			if (gi->hasInitializer())
				extract_from_consts(gi->getInitializer());
		}
	}
	
	// Instructions and their constant operands. 
	forallinst(M, ii) {
		if (!EO.not_executed(ii) &&
				(isa<IntegerType>(ii->getType()) || isa<PointerType>(ii->getType())))
			integers.insert(ii);
		// No matter reachable or not, capture its constant operands. 
		for (unsigned i = 0; i < ii->getNumOperands(); ++i) {
			if (Constant *c = dyn_cast<Constant>(ii->getOperand(i)))
				extract_from_consts(c);
		}
	}
	
	// Function parameters. 
	forallfunc(M, fi) {
		if (EO.not_executed(fi))
			continue;
		for (Function::arg_iterator ai = fi->arg_begin();
				ai != fi->arg_end(); ++ai) {
			if (isa<IntegerType>(ai->getType()) || isa<PointerType>(ai->getType()))
				integers.insert(ai);
		}
	}
}

void CaptureConstraints::capture_top_level(Module &M) {

	// Users: Instructions or ConstantExprs
	forall(ConstValueSet, it, integers) {
		if (const User *user = dyn_cast<User>(*it))
			add_constraint(get_in_user(user));
	}
	
	// Function parameters: formal = actual
	forall(ConstValueSet, it, integers) {
		if (const Argument *arg = dyn_cast<Argument>(*it))
			add_constraint(get_in_argument(arg));
	}
}

Clause *CaptureConstraints::get_in_argument(const Argument *formal) {
	
	CallGraphFP &CG = getAnalysis<CallGraphFP>();
	ExecOnce &EO = getAnalysis<ExecOnce>();

	const Function *f = formal->getParent();
	// Needn't handle function declarations.
	if (f->isDeclaration())
		return NULL;
	
	InstList call_sites = CG.get_call_sites(f);
	ConstValueSet actuals;
	for (size_t j = 0; j < call_sites.size(); ++j) {
		if (!EO.not_executed(call_sites[j])) {
			CallSite cs(call_sites[j]);
			// Matches the formal argument with the actual argument. 
			const Value *actual;
			if (is_pthread_create(cs.getInstruction()))
				actual = get_pthread_create_arg(cs.getInstruction());
			else
				actual = cs.getArgument(formal->getArgNo());
			actuals.insert(actual);
		}
	}

	Clause *disj = NULL;
	forall(ConstValueSet, it, actuals) {
		const Value *actual = *it;
		Clause *c = new Clause(new BoolExpr(
					CmpInst::ICMP_EQ, new Expr(formal), new Expr(actual)));
		if (disj == NULL)
			disj = c;
		else
			disj = new Clause(Instruction::Or, disj, c);
	}
	return disj;
}

Clause *CaptureConstraints::get_in_user(const User *user) {
	assert(is_integer(user));
	unsigned opcode = Operator::getOpcode(user);
	if (opcode == Instruction::UserOp1)
		return NULL;
	switch (opcode) {
		// Binary instructions
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
			return get_in_binary(user, opcode);
			// ICmpInst
		case Instruction::ICmp:
			assert(isa<ICmpInst>(user));
			return get_in_icmp(cast<ICmpInst>(user));
			// Unary Instructions
		case Instruction::Trunc:
		case Instruction::ZExt:
		case Instruction::SExt:
		case Instruction::PtrToInt:
		case Instruction::IntToPtr:
		case Instruction::BitCast:
			return get_in_unary(user);
		case Instruction::GetElementPtr:
			return get_in_gep(user);
		case Instruction::Select:
			assert(isa<SelectInst>(user));
			return get_in_select(cast<SelectInst>(user));
		case Instruction::PHI:
			return get_in_phi(dyn_cast<PHINode>(user));
		default:
			return NULL;
	}
}

Clause *CaptureConstraints::get_in_select(const SelectInst *si) {

	// TODO: Handle the case where the condition is a vector of i1. 
	// FIXME: We are assuming the true value and the false value are
	// exclusive. Not always true. 
	const Value *cond = si->getCondition();
	const Value *true_value = si->getTrueValue();
	const Value *false_value = si->getFalseValue();
	if (!is_integer(cond) || !is_integer(true_value) || !is_integer(false_value))
		return NULL;
	if (!cond->getType()->isIntegerTy(1))
		return NULL;

	// cond == 1 <==> si == true value
	// cond == 0 <==> si == false value
	// ==>
	// (cond == 0) ^ (si == true value)
	// (cond == 1) ^ (si == false value)
	Clause *c_true = new Clause(Instruction::Xor,
			new Clause(new BoolExpr(CmpInst::ICMP_EQ,
					new Expr(cond), new Expr(ConstantInt::getFalse(si->getContext())))),
			new Clause(new BoolExpr(CmpInst::ICMP_EQ,
					new Expr(si), new Expr(true_value))));
	Clause *c_false = new Clause(Instruction::Xor,
			new Clause(new BoolExpr(CmpInst::ICMP_EQ,
					new Expr(cond), new Expr(ConstantInt::getTrue(si->getContext())))),
			new Clause(new BoolExpr(CmpInst::ICMP_EQ,
					new Expr(si), new Expr(false_value))));
	return new Clause(Instruction::And, c_true, c_false);
}

Clause *CaptureConstraints::get_in_unary(const User *u) {
	assert(u->getNumOperands() == 1);
	const Value *v = u->getOperand(0);
	if (!integers.count(v))
		return NULL;
	// u == v, but they may have different bit widths. 
	unsigned opcode = Operator::getOpcode(u);
	assert(opcode != Instruction::UserOp1);
	Expr *eu = new Expr(u), *ev = new Expr(v);
	// TODO: e->get_width() may not equal the real bit width, because we
	// don't distinguish 32-bit and 64-bit integers currently.
	if (eu->get_width() > ev->get_width()) {
		assert(opcode == Instruction::SExt || opcode == Instruction::ZExt);
		ev = new Expr(opcode, ev);
	} else if (eu->get_width() < ev->get_width()) {
		assert(opcode == Instruction::Trunc);
		ev = new Expr(opcode, ev);
	}
	assert(eu->get_width() == ev->get_width());
	return new Clause(new BoolExpr(CmpInst::ICMP_EQ, eu, ev));
}

Clause *CaptureConstraints::get_in_icmp(const ICmpInst *icmp) {
	/*
	 * FIXME: We are assuming the true value and the false value are
	 * exclusive. Not always true. 
	 */
	// icmp == 1 <==> branch holds
	// NOTE: <icmp> will be translated to one STP bit. Therefore, 
	// it's either 0 or 1. 
	// i.e. (icmp == 0) ^ (branch holds) == 1
	const Value *op0 = icmp->getOperand(0);
	const Value *op1 = icmp->getOperand(1);
	if (!is_integer(op0) || !is_integer(op1))
		return NULL;
	Clause *branch = new Clause(new BoolExpr(
				icmp->getPredicate(), new Expr(op0), new Expr(op1)));
	ConstantInt *f = ConstantInt::getFalse(icmp->getContext());
	assert(icmp->getType() == f->getType());
	return new Clause(Instruction::Xor,
			new Clause(new BoolExpr(CmpInst::ICMP_EQ, new Expr(icmp), new Expr(f))),
			branch);
}

Clause *CaptureConstraints::get_in_phi(const PHINode *phi) {

	// Check the loop depths. 
	// Each incoming edge must be from a shallower loop or a loop from
	// the same depth but not a backedge. 
	for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
		if (!comes_from_shallow(phi->getIncomingBlock(i), phi->getParent()))
			return NULL;
	}

	Clause *disj = NULL;
	for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
		Clause *c = new Clause(new BoolExpr(
					CmpInst::ICMP_EQ,
					new Expr(phi),
					new Expr(phi->getIncomingValue(i))));
		if (!disj)
			disj = c;
		else
			disj = new Clause(Instruction::Or, disj, c);
	}
	assert(disj && "Empty PHINode");
	return disj;
}

Clause *CaptureConstraints::get_in_binary(const User *user, unsigned opcode) {
	assert(user->getNumOperands() == 2);
	if (!integers.count(user->getOperand(0)))
		return NULL;
	if (!integers.count(user->getOperand(1)))
		return NULL;
	Expr *e1 = new Expr(user);
	Expr *e2 = new Expr(
			opcode,
			new Expr(user->getOperand(0)),
			new Expr(user->getOperand(1)));
	// FIXME: Dirty hack. 
	// Module the shift width by 32. 
	if (opcode == Instruction::Shl || opcode == Instruction::LShr ||
				opcode == Instruction::AShr) {
		if (ConstantInt *ci = dyn_cast<ConstantInt>(user->getOperand(1))) {
			uint64_t shift_width = ci->getZExtValue();
			shift_width %= 32;
			delete e2->e2;
			e2->e2 = new Expr(ConstantInt::get(int_type, shift_width));
		}
	}
	// FIXME: Dirty hack. 
	// Treat any operand in a Mul instruction like n * 2 ^ 32 as n.
	if (opcode == Instruction::Mul) {
		for (unsigned i = 0; i < user->getNumOperands(); ++i) {
			if (ConstantInt *ci = dyn_cast<ConstantInt>(user->getOperand(i))) {
				int64_t v = ci->getSExtValue();
				if (v % (1LL << 32) == 0)
					v /= (1LL << 32);
				Expr *&the_e = (i == 0 ? e2->e1 : e2->e2);
				delete the_e;
				the_e = new Expr(ConstantInt::get(int_type, v));
			}
		}
	}

	return new Clause(new BoolExpr(CmpInst::ICMP_EQ, e1, e2));
}

bool CaptureConstraints::is_power_of_two(uint64_t a, uint64_t &e) {
	if (a <= 0)
		return false;
	e = 0;
	while (a % 2 == 0) {
		a /= 2;
		++e;
	}
	return a == 1;
}

Clause *CaptureConstraints::get_in_gep(const User *user) {
	for (unsigned i = 0; i < user->getNumOperands(); ++i) {
		if (!integers.count(user->getOperand(i)))
			return NULL;
	}
	const Value *base = user->getOperand(0);
	Expr *cur = new Expr(base);
	// <cur> and <type> need be consistent. 
	// <type> is the type of the item that <cur> points to. 
	const Type *type = base->getType();
	TargetData &TD = getAnalysis<TargetData>();
	for (unsigned i = 1; i < user->getNumOperands(); ++i) {
		if (const SequentialType *sqt = dyn_cast<SequentialType>(type)) {
			const Type *et = sqt->getElementType();
			uint64_t type_size = TD.getTypeSizeInBits(et);
			uint64_t exp = 0;
			Expr *delta;
			if (is_power_of_two(type_size, exp)) {
				delta = new Expr(Instruction::Shl,
						new Expr(user->getOperand(i)),
						new Expr(ConstantInt::get(int_type, exp)));
			} else {
				delta = new Expr(Instruction::Mul,
						new Expr(ConstantInt::get(int_type, type_size)),
						new Expr(user->getOperand(i)));
			}
			cur = new Expr(Instruction::Add, cur, delta);
			type = et;
		} else if (const StructType *st = dyn_cast<StructType>(type)) {
			ConstantInt *idx = dyn_cast<ConstantInt>(user->getOperand(i));
			assert(idx && "Not supported");
			unsigned m = idx->getZExtValue();
			assert(m < st->getNumElements());
			unsigned offset = 0;
			for (unsigned j = 0; j < m; ++j)
				offset += TD.getTypeSizeInBits(st->getElementType(j));
			Expr *delta = new Expr(ConstantInt::get(int_type, offset));
			cur = new Expr(Instruction::Add, cur, delta);
			type = st->getElementType(m);
		} else {
			assert(false && "Not supported");
		}
	}
	return new Clause(new BoolExpr(
				CmpInst::ICMP_EQ, new Expr(user), cur));
}

bool CaptureConstraints::is_constant_integer(const Value *v) const {
	
	ExecOnce &EO = getAnalysis<ExecOnce>();

	if (!is_integer(v))
		return false;
	if (const Instruction *ins = dyn_cast<Instruction>(v))
		return EO.executed_once(ins);
	return true;
}
