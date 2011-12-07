/**
 * Author: Jingyue
 */

#include "llvm/Target/TargetData.h"
using namespace llvm;

#include "common/util.h"
using namespace rcs;

#include "slicer/capture.h"
using namespace slicer;

void CaptureConstraints::capture_function_summaries(Module &M) {
	// Capture all memory allocations along the way. 
	vector<pair<Expr *, Expr *> > blocks;
	
	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
			for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
				// memory allocations. e.g. alloca, malloc, valloc, calloc. 
				Expr *start = NULL, *size = NULL;
				if (capture_memory_allocation(ins, start, size))
					blocks.push_back(make_pair(start, size));
				// libcall summaries
				CallSite cs(ins);
				if (cs.getInstruction())
					capture_libcall(cs);
			}
		}
	}

	// Handle memory allocations. 
	for (size_t i = 0; i < blocks.size(); ++i) {
		add_constraint(new Clause(new BoolExpr(CmpInst::ICMP_SLE,
						blocks[i].first,
						new Expr(Instruction::Sub,
							new Expr(ConstantInt::get(int_type, INT_MAX)),
							blocks[i].second))));
	}
	for (size_t i = 0; i + 1 < blocks.size(); ++i) {
		// start[i] + size[i] <= start[i + 1]
		add_constraint(new Clause(new BoolExpr(CmpInst::ICMP_SLE,
						new Expr(Instruction::Add, blocks[i].first, blocks[i].second),
						blocks[i + 1].first)));
	}
}

bool CaptureConstraints::capture_memory_allocation(Instruction *ins,
		Expr *&start, Expr *&size) {
	// alloca: they are not function calls
	if (AllocaInst *ai = dyn_cast<AllocaInst>(ins)) {
		if (!is_reachable_integer(ins))
			return false;
		TargetData &TD = getAnalysis<TargetData>();
		uint64_t size_in_bits = TD.getTypeSizeInBits(ai->getAllocatedType());
		assert(size_in_bits % 8 == 0);
		start = new Expr(ai);
		size = new Expr(ConstantInt::get(int_type, size_in_bits / 8));
		return true;
	}

	CallSite cs(ins);
	if (!cs.getInstruction())
		return false;

	Function *callee = cs.getCalledFunction();
	if (!callee)
		return false;
	
	string name = callee->getName();
	if (name == "malloc" || name == "valloc") {
		if (!is_reachable_integer(ins))
			return false;
		// TODO: valloc also guarantees the block is page-aligned. 
		assert(cs.arg_size() == 1);
		start = new Expr(cs.getInstruction());
		size = new Expr(cs.getArgument(0));
		return true;
	} 
	
	if (name == "calloc") {
		if (!is_reachable_integer(ins))
			return false;
		assert(cs.arg_size() == 2);
		start = new Expr(cs.getInstruction());
		size = new Expr(Instruction::Mul,
					new Expr(cs.getArgument(0)),
					new Expr(cs.getArgument(1)));
		return true;
	}

	return false;
}

void CaptureConstraints::capture_libcall(CallSite &cs) {
	Function *callee = cs.getCalledFunction();
	if (!callee)
		return;
	string name = callee->getName();
	
	Constant *zero = ConstantInt::get(int_type, 0);
	Constant *minus_one = ConstantInt::getSigned(int_type, -1);
	if (name == "pwrite") {
		// ret = pwrite(???, ???, len, offset)
		// ret >= 0
		// if len >= 0, ret <= len
		// FIXME: >= -1. But aget has a bug no checking its return value. 
		Instruction *ret = cs.getInstruction();
		if (is_reachable_integer(ret)) {
			add_constraint(new Clause(new BoolExpr(CmpInst::ICMP_SGE,
							new Expr(ret), new Expr(minus_one))));
			Value *len = cs.getArgument(2);
			if (is_reachable_integer(len)) {
				// len >= 0 ==> ret <= len
				// i.e
				// len < 0 or ret <= len
				add_constraint(new Clause(Instruction::Or,
							new Clause(new BoolExpr(CmpInst::ICMP_SLT,
									new Expr(len), new Expr(zero))),
							new Clause(new BoolExpr(CmpInst::ICMP_SLE,
									new Expr(ret), new Expr(len)))));
			}
		}
	}
	if (name == "recv") {
		// ret = recv(???, ???, len, ???)
		// ret >= -1
		// len >= 0 ==> ret <= len i.e. len < 0 or ret <= len
		Value *ret = cs.getInstruction();
		if (is_reachable_integer(ret)) {
			Value *len = cs.getArgument(2);
			add_constraint(new Clause(new BoolExpr(CmpInst::ICMP_SGE,
							new Expr(ret), new Expr(minus_one))));
			if (is_reachable_integer(len)) {
				add_constraint(new Clause(Instruction::Or,
							new Clause(new BoolExpr(CmpInst::ICMP_SLT,
									new Expr(len), new Expr(zero))),
							new Clause(new BoolExpr(CmpInst::ICMP_SLE,
									new Expr(ret), new Expr(len)))));
			}
		}
	}
	if (name == "rand") {
		// ret >= 0
		Value *ret = cs.getInstruction();
		if (is_reachable_integer(ret)) {
			add_constraint(new Clause(new BoolExpr(CmpInst::ICMP_SGE,
							new Expr(ret), new Expr(ConstantInt::get(int_type, 0)))));
		}
	}
}
