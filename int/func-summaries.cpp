/**
 * Author: Jingyue
 */

#include "common/include/util.h"
using namespace llvm;

#include "capture.h"
using namespace slicer;

void CaptureConstraints::capture_func_summaries(Module &M) {
	
	// Capture all memory allocations along the way. 
	vector<pair<Expr *, Expr *> > blocks;
	
	forallinst(M, ii) {

		CallSite cs = CallSite::get(ii);
		if (!cs.getInstruction())
			continue;
		
		capture_libcall(cs);

		Expr *start = NULL, *size = NULL;
		if (capture_memory_allocation(cs, start, size))
			blocks.push_back(make_pair(start, size));
	}

	// Handle memory allocations. 
	for (size_t i = 0; i < blocks.size(); ++i) {
		constraints.push_back(new Clause(new BoolExpr(CmpInst::ICMP_SLE,
						blocks[i].first,
						new Expr(Instruction::Sub,
							new Expr(ConstantInt::get(int_type, INT_MAX)),
							blocks[i].second))));
	}
	for (size_t i = 0; i + 1 < blocks.size(); ++i) {
		// start[i] + size[i] <= start[i + 1]
		constraints.push_back(new Clause(new BoolExpr(CmpInst::ICMP_SLE,
						new Expr(Instruction::Add, blocks[i].first, blocks[i].second),
						blocks[i + 1].first)));
	}
}

bool CaptureConstraints::capture_memory_allocation(
		const CallSite &cs, Expr *&start, Expr *&size) {

	const Function *callee = cs.getCalledFunction();
	if (!callee)
		return false;
	
	const string &name = callee->getNameStr();
	if (name == "malloc" || name == "valloc") {
		// TODO: valloc also guarantees the block is page-aligned. 
		assert(cs.arg_size() == 1);
		start = new Expr(cs.getInstruction());
		size = new Expr(Instruction::Shl,
				new Expr(cs.getArgument(0)),
				new Expr(ConstantInt::get(int_type, 3)));
		return true;
	} 
	
	if (name == "calloc") {
		assert(cs.arg_size() == 2);
		start = new Expr(cs.getInstruction());
		size = new Expr(Instruction::Shl,
				new Expr(Instruction::Mul,
					new Expr(cs.getArgument(0)),
					new Expr(cs.getArgument(1))),
				new Expr(ConstantInt::get(int_type, 3)));
		return true;
	}

	return false;
}

void CaptureConstraints::capture_libcall(const CallSite &cs) {

	const Function *callee = cs.getCalledFunction();
	if (!callee)
		return;
	
	const string &name = callee->getNameStr();
	if (name == "pwrite") {
		// The return value >= 0.
		// FIXME: >= -1. But aget has a bug no checking its return value. 
		const Instruction *ret = cs.getInstruction();
		if (is_integer(ret)) {
			constraints.push_back(new Clause(new BoolExpr(CmpInst::ICMP_SGE,
							new Expr(ret), new Expr(ConstantInt::get(int_type, 0)))));
		}
	}
}
