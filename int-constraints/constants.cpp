#include "common/callgraph-fp/callgraph-fp.h"
using namespace llvm;

#include "capture.h"
#include "exec-once.h"

namespace slicer {

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
			if (isa<IntegerType>(gi->getType()) || isa<PointerType>(gi->getType()))
				constants.insert(gi);
		}
		// Instructions and their constant operands. 
		forallinst(M, ii) {
			if (EO.executed_once(ii) &&
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
			if (!EO.executed_once(fi))
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
				CallGraphFP::SiteList call_sites = CG.get_call_sites(arg->getParent());
				// A function may have no callers after slicing. 
				assert(call_sites.size() <= 1 && "Otherwise it shouldn't be a constant");
				if (call_sites.size() == 1) {
					Instruction *call_site = call_sites[0];
					// Matches the formal argument with the actual argument. 
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
		if (!isa<Instruction>(user) && !isa<ConstantExpr>(user))
			return;
		bool all_const = true;
		for (unsigned i = 0; i < user->getNumOperands(); ++i) {
			if (!constants.count(user->getOperand(i))) {
				all_const = false;
				break;
			}
		}
		// We can't infer anything if some operands are not constant. 
		if (!all_const)
			return;
		unsigned opcode;
		if (Instruction *ins = dyn_cast<Instruction>(user))
			opcode = ins->getOpcode();
		else
			opcode = dyn_cast<ConstantExpr>(user)->getOpcode();
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
			// Unary Instructions
			case Instruction::Trunc:
			case Instruction::ZExt:
			case Instruction::SExt:
			case Instruction::PtrToInt:
			case Instruction::IntToPtr:
			case Instruction::BitCast:
				assert(user->getNumOperands() == 1);
				add_eq_constraint(user, user->getOperand(0));
				break;
			case Instruction::GetElementPtr:
				capture_in_gep(user);
				break;
			// TODO: handle PHINodes
		}
	}

	void CaptureConstraints::capture_in_binary(User *user, unsigned opcode) {
		assert(user->getNumOperands() == 2);
		Expr *e1 = new Expr(user);
		Expr *e2 = new Expr(
				opcode,
				new Expr(user->getOperand(0)),
				new Expr(user->getOperand(1)));
		BoolExpr *be = new BoolExpr(CmpInst::ICMP_EQ, e1, e2);
		constraints.push_back(Clause::create_bool_expr(be));
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
		type->print(errs());
		assert(false && "The type is not sized");
	}

	void CaptureConstraints::capture_in_gep(User *user) {
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
				for (unsigned j = 0; j < m; ++j) {
					Expr *delta = new Expr(ConstantInt::get(
								int_type,
								get_type_size(st->getElementType(j))));
					cur = new Expr(Instruction::Add, cur, delta);
				}
				type = st->getElementType(m);
			} else {
				assert(false && "Not supported");
			}
		}
	}

	void CaptureConstraints::add_eq_constraint(Value *v1, Value *v2) {
		BoolExpr *be = new BoolExpr(CmpInst::ICMP_EQ, new Expr(v1), new Expr(v2));
		constraints.push_back(Clause::create_bool_expr(be));
	}
}
