#include "llvm/LLVMContext.h"
#include "llvm/Target/TargetData.h"
#include "idm/id.h"
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
	/*
	 * Constants:
	 * - Constants (global vars, constant exprs, ...)
	 * - Instructions
	 * - Function parameters
	 */
	forall(ConstValueSet, it, integers) {
		if (const Argument *arg = dyn_cast<Argument>(*it)) {
			capture_from_argument(arg);
		} else if (const User *user = dyn_cast<User>(*it)) {
			capture_from_user(user);
		} else {
			assert(false && "Not supported");
		}
	}
}

void CaptureConstraints::capture_from_argument(const Argument *arg) {
	
	CallGraphFP &CG = getAnalysis<CallGraphFP>();
	ExecOnce &EO = getAnalysis<ExecOnce>();

	const Function *f = arg->getParent();
	// Needn't handle function declarations.
	if (f->isDeclaration())
		return;
	
	InstList call_sites = CG.get_call_sites(arg->getParent());
	Clause *disj = NULL;
	for (size_t j = 0; j < call_sites.size(); ++j) {
		if (!EO.not_executed(call_sites[j])) {
			const Instruction *call_site = call_sites[j];
			// Matches the formal argument with the actual argument. 
			// TODO: The order of operands of CallInst/InvokeInst is changed
			// in later version. 
			const Value *param;
			if (is_pthread_create(call_site)) {
				// pthread_create(thread, attr, func, arg)
				assert(arg->getArgNo() == 0);
				assert(call_site->getNumOperands() == 5);
				assert(integers.count(call_site->getOperand(4)));
				param = call_site->getOperand(4);
			} else {
				param = call_site->getOperand(arg->getArgNo() + 1);
			}
			Clause *c = new Clause(new BoolExpr(
						CmpInst::ICMP_EQ, new Expr(arg), new Expr(param)));
			if (disj == NULL)
				disj = c;
			else
				disj = new Clause(Instruction::Or, disj, c);
		}
	}
	if (!disj)
		errs() << "[Warning] Function " << f->getName() << " is unreachable.\n";
	if (disj)
		constraints.push_back(disj);
}

void CaptureConstraints::capture_from_user(const User *user) {
	assert(is_integer(user));
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

void CaptureConstraints::capture_in_unary(const User *u) {
	assert(u->getNumOperands() == 1);
	const Value *v = u->getOperand(0);
	if (!integers.count(v))
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

void CaptureConstraints::capture_in_icmp(const ICmpInst *icmp) {
	// icmp == 1 <==> branch holds
	// NOTE: <icmp> will be translated to one STP bit. Therefore, 
	// it's either 0 or 1. 
	// i.e. (icmp == 0) ^ (branch holds) == 1
	const Value *op0 = icmp->getOperand(0);
	const Value *op1 = icmp->getOperand(1);
	if (!is_integer(op0) || !is_integer(op1))
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

bool CaptureConstraints::comes_from_shallow(
		const BasicBlock *x, const BasicBlock *y) {
	assert(x->getParent() == y->getParent());
	const Function *f = x->getParent();
	LoopInfo &LI = getAnalysis<LoopInfo>(*const_cast<Function *>(f));
	if (LI.getLoopDepth(x) < LI.getLoopDepth(y))
		return true;
	if (LI.getLoopDepth(x) > LI.getLoopDepth(y))
		return false;
	if (LI.isLoopHeader(const_cast<BasicBlock *>(y))) {
		const Loop *ly = LI.getLoopFor(y);
		assert(ly);
		if (ly->contains(x))
			return false;
	}
	return true;
}

void CaptureConstraints::capture_in_phi(const PHINode *phi) {

	// Check the loop depths. 
	// Each incoming edge must be from a shallower loop or a loop from
	// the same depth but not a backedge. 
	for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
		if (!comes_from_shallow(phi->getIncomingBlock(i), phi->getParent()))
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

void CaptureConstraints::capture_in_binary(const User *user, unsigned opcode) {
	assert(user->getNumOperands() == 2);
	if (!integers.count(user->getOperand(0)))
		return;
	if (!integers.count(user->getOperand(1)))
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

void CaptureConstraints::capture_in_gep(const User *user) {
	for (unsigned i = 0; i < user->getNumOperands(); ++i) {
		if (!integers.count(user->getOperand(i)))
			return;
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
			Expr *delta = new Expr(
					Instruction::Mul,
					new Expr(ConstantInt::get(int_type, TD.getTypeSizeInBits(et))),
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
				offset += TD.getTypeSizeInBits(st->getElementType(j));
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

void CaptureConstraints::add_eq_constraint(const Value *v1, const Value *v2) {
	BoolExpr *be = new BoolExpr(CmpInst::ICMP_EQ, new Expr(v1), new Expr(v2));
	constraints.push_back(new Clause(be));
}
