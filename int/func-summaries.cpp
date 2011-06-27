/**
 * Author: Jingyue
 */

#include "capture.h"
using namespace slicer;

void CaptureConstraints::capture_func_summaries(Module &M) {
	forallinst(M, ii) {
		CallSite cs = CallSite::get(ii);
		if (!cs.getInstruction())
			continue;
		capture_libcall(cs);
	}
}

void CaptureConstraints::capture_libcall(const CallSite &cs) {

	const Function *callee = cs.getCalledFunction();
	if (!callee)
		return;
	
	const string &name = callee->getNameStr();
	if (name == "pwrite") {
		// The return value >= 0.
		const Instruction *ret = cs.getInstruction();
		if (is_constant(ret)) {
			constraints.push_back(new Clause(new BoolExpr(
							CmpInst::ICMP_SGE,
							new Expr(ret),
							new Expr(ConstantInt::get(int_type, 0)))));
		}
	}
}
