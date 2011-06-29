#include "llvm/LLVMContext.h"
#include "idm/id.h"
#include "common/callgraph-fp/callgraph-fp.h"
#include "common/cfg/exec-once.h"
using namespace llvm;

#include "capture.h"
using namespace slicer;

void CaptureConstraints::extract_consts(Constant *c) {
	// We don't care about functions. 
	// We might unlikely in the future, when we want to reason about integer
	// constraints on function pointers. 
	if (isa<Function>(c))
		return;
	// Only handle integer constants and pointer constants. 
	if (!isa<IntegerType>(c->getType()) && !isa<PointerType>(c->getType()))
		return;
	constants.insert(c);
	for (unsigned i = 0; i < c->getNumOperands(); ++i) {
		if (Constant *ct = dyn_cast<Constant>(c->getOperand(i)))
			extract_consts(ct);
	}
}

void CaptureConstraints::identify_constants(Module &M) {
	constants.clear();
	ExecOnce &EO = getAnalysis<ExecOnce>();
	// Global variables. 
	for (Module::global_iterator gi = M.global_begin();
			gi != M.global_end(); ++gi) {
		if (isa<IntegerType>(gi->getType()) || isa<PointerType>(gi->getType())) {
			constants.insert(gi);
			if (gi->hasInitializer())
				extract_consts(gi->getInitializer());
		}
	}
	// Instructions and their constant operands. 
	forallinst(M, ii) {
		if (EO.executed_once(ii) && !EO.not_executed(ii) &&
				(isa<IntegerType>(ii->getType()) || isa<PointerType>(ii->getType())))
			constants.insert(ii);
		// Constant expressions. 
		for (unsigned i = 0; i < ii->getNumOperands(); ++i) {
			if (Constant *c = dyn_cast<Constant>(ii->getOperand(i)))
				extract_consts(c);
		}
	}
	// Function parameters. 
	forallfunc(M, fi) {
		if (!EO.executed_once(fi) || EO.not_executed(fi))
			continue;
		for (Function::arg_iterator ai = fi->arg_begin();
				ai != fi->arg_end(); ++ai) {
			if (isa<IntegerType>(ai->getType()) || isa<PointerType>(ai->getType()))
				constants.insert(ai);
		}
	}
}

void CaptureConstraints::capture_constraints_on_consts(Module &M) {
	/*
	 * Constants:
	 * - Constants (global vars, constant exprs, ...)
	 * - Instructions
	 * - Function parameters
	 */
	forall(ValueSet, it, constants) {
		if (Argument *arg = dyn_cast<Argument>(*it)) {
			CallGraphFP &CG = getAnalysis<CallGraphFP>();
			InstList call_sites = CG.get_call_sites(arg->getParent());
			// A function may have no callers after slicing. 
			assert(call_sites.size() <= 1 && "Otherwise it shouldn't be a constant");
			if (call_sites.size() == 1) {
				Instruction *call_site = call_sites[0];
				// Matches the formal argument with the actual argument. 
				// TODO: The order of operands of CallInst/InvokeInst is changed
				// in later version. 
				if (is_pthread_create(call_site)) {
					// pthread_create(thread, attr, func, arg)
					assert(arg->getArgNo() == 0);
					assert(call_site->getNumOperands() == 5);
					assert(constants.count(call_site->getOperand(4)));
					add_eq_constraint(arg, call_site->getOperand(4));
				} else {
					add_eq_constraint(arg, call_site->getOperand(arg->getArgNo() + 1));
				}
			}
		} else if (User *user = dyn_cast<User>(*it)) {
			capture_in_user(user);
		} else {
			assert(false && "Not supported");
		}
	}
}

void CaptureConstraints::capture_in_user(User *user) {
	assert(is_constant(user));
	unsigned opcode = Operator::getOpcode(user);
	if (opcode == Instruction::UserOp1)
		return;
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
			capture_in_binary(user, opcode);
			break;
			// ICmpInst
		case Instruction::ICmp:
			assert(isa<ICmpInst>(user));
			capture_in_icmp(dyn_cast<ICmpInst>(user));
			break;
			// Unary Instructions
		case Instruction::Trunc:
		case Instruction::ZExt:
		case Instruction::SExt:
		case Instruction::PtrToInt:
		case Instruction::IntToPtr:
		case Instruction::BitCast:
			capture_in_unary(user);
			break;
		case Instruction::GetElementPtr:
			capture_in_gep(user);
			break;
		case Instruction::PHI:
			capture_in_phi(dyn_cast<PHINode>(user));
			break;
	}
}

void CaptureConstraints::capture_in_unary(User *u) {
	assert(u->getNumOperands() == 1);
	Value *v = u->getOperand(0);
	if (!constants.count(v))
		return;
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
	constraints.push_back(new Clause(new BoolExpr(CmpInst::ICMP_EQ, eu, ev)));
}

void CaptureConstraints::capture_in_icmp(ICmpInst *icmp) {
	// icmp == 1 <==> branch holds
	// NOTE: <icmp> will be translated to one STP bit. Therefore, 
	// it's either 0 or 1. 
	// i.e. (icmp == 0) ^ (branch holds) == 1
	const Value *op0 = icmp->getOperand(0);
	const Value *op1 = icmp->getOperand(1);
	if (!is_constant(op0) || !is_constant(op1))
		return;
	Clause *branch = new Clause(new BoolExpr(
				icmp->getPredicate(), new Expr(op0), new Expr(op1)));
	ConstantInt *f = ConstantInt::getFalse(getGlobalContext());
	assert(icmp->getType() == f->getType());
	constraints.push_back(new Clause(
				Instruction::Xor,
				new Clause(new BoolExpr(
						CmpInst::ICMP_EQ, new Expr(icmp), new Expr(f))),
				branch));
}

void CaptureConstraints::capture_in_phi(PHINode *phi) {
	for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
		if (!constants.count(phi->getIncomingValue(i)))
			return;
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
	constraints.push_back(disj);
}

void CaptureConstraints::capture_in_binary(User *user, unsigned opcode) {
	assert(user->getNumOperands() == 2);
	if (!constants.count(user->getOperand(0)))
		return;
	if (!constants.count(user->getOperand(1)))
		return;
	Expr *e1 = new Expr(user);
	Expr *e2 = new Expr(
			opcode,
			new Expr(user->getOperand(0)),
			new Expr(user->getOperand(1)));
	// FIXME: Dirty hack. 
	// If it's a shift by 32, treat it as shift by 0. 
	if ((opcode == Instruction::Shl || opcode == Instruction::LShr ||
				opcode == Instruction::AShr)) {
		if (ConstantInt *ci = dyn_cast<ConstantInt>(user->getOperand(1))) {
			if (ci->getSExtValue() == 32) {
				delete e2->e2;
				e2->e2 = new Expr(
						ConstantInt::get(IntegerType::get(getGlobalContext(), 32), 0));
			}
		}
	}
	BoolExpr *be = new BoolExpr(CmpInst::ICMP_EQ, e1, e2);
	constraints.push_back(new Clause(be));
}

unsigned CaptureConstraints::get_type_size(const Type *type) {
	assert(type->isSized() && "The type is not sized");
	if (type->isPrimitiveType()) {
		unsigned ret = type->getPrimitiveSizeInBits();
		assert(ret > 0);
		return ret;
	}
	if (const IntegerType *it = dyn_cast<IntegerType>(type))
		return it->getBitWidth();
	// TODO: sure?
	if (isa<PointerType>(type))
		return __WORDSIZE;
	if (const ArrayType *at = dyn_cast<ArrayType>(type))
		return at->getNumElements() * get_type_size(at->getElementType());
	if (const VectorType *vt = dyn_cast<VectorType>(type))
		return vt->getNumElements() * get_type_size(vt->getElementType());
	if (const StructType *st = dyn_cast<StructType>(type)) {
		unsigned ret = 0;
		for (unsigned i = 0; i < st->getNumElements(); ++i)
			ret += get_type_size(st->getElementType(i));
		return ret;
	}
	if (const UnionType *ut = dyn_cast<UnionType>(type)) {
		unsigned ret = 0;
		for (unsigned i = 0; i < ut->getNumElements(); ++i)
			ret = max(ret, get_type_size(ut->getElementType(i)));
		return ret;
	}
	assert(false && "The type is not sized");
}

void CaptureConstraints::capture_in_gep(User *user) {
	for (unsigned i = 0; i < user->getNumOperands(); ++i) {
		if (!constants.count(user->getOperand(i)))
			return;
	}
	Value *base = user->getOperand(0);
	Expr *cur = new Expr(base);
	// <cur> and <type> need be consistent. 
	// <type> is the type of the item that <cur> points to. 
	const Type *type = base->getType();
	for (unsigned i = 1; i < user->getNumOperands(); ++i) {
		if (const SequentialType *sqt = dyn_cast<SequentialType>(type)) {
			const Type *et = sqt->getElementType();
			Expr *delta = new Expr(
					Instruction::Mul,
					new Expr(ConstantInt::get(int_type, get_type_size(et))),
					new Expr(user->getOperand(i)));
			cur = new Expr(Instruction::Add, cur, delta);
			type = et;
		} else if (const StructType *st = dyn_cast<StructType>(type)) {
			ConstantInt *idx = dyn_cast<ConstantInt>(user->getOperand(i));
			assert(idx && "Not supported");
			unsigned m = idx->getZExtValue();
			assert(m < st->getNumElements());
			unsigned offset = 0;
			for (unsigned j = 0; j < m; ++j)
				offset += get_type_size(st->getElementType(j));
			Expr *delta = new Expr(ConstantInt::get(int_type, offset));
			cur = new Expr(Instruction::Add, cur, delta);
			type = st->getElementType(m);
		} else {
			assert(false && "Not supported");
		}
	}
	constraints.push_back(new Clause(new BoolExpr(
					CmpInst::ICMP_EQ, new Expr(user), cur)));
}

void CaptureConstraints::add_eq_constraint(Value *v1, Value *v2) {
	BoolExpr *be = new BoolExpr(CmpInst::ICMP_EQ, new Expr(v1), new Expr(v2));
	constraints.push_back(new Clause(be));
}

bool CaptureConstraints::is_constant(const Value *v) const {
	return constants.count(const_cast<Value *>(v));
}
