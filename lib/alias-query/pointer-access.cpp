/**
 * Author: Jingyue
 */

#include "llvm/Instructions.h"
using namespace llvm;

#include "common/util.h"
using namespace rcs;

#include "pointer-access.h"
using namespace slicer;

PointerAccess::PointerAccess(const Instruction *acc, const Value *p, bool write)
	: accessor(acc), loc(p), is_write(write) {}

namespace slicer {
	vector<PointerAccess> get_pointer_accesses(const Instruction *ins) {
		vector<PointerAccess> result;
		if (const StoreInst *si = dyn_cast<StoreInst>(ins)) {
			result.push_back(PointerAccess(si, si->getPointerOperand(), true));
		} else if (const LoadInst *li = dyn_cast<LoadInst>(ins)) {
			result.push_back(PointerAccess(li, li->getPointerOperand(), false));
		} else if (is_call(ins)) {
			CallSite cs(const_cast<Instruction *>(ins));
			Function *callee = cs.getCalledFunction();
			if (callee) {
				if (callee->getName() == "read") {
					result.push_back(PointerAccess(ins, cs.getArgument(1), true));
				}
				if (callee->getName() == "write") {
					result.push_back(PointerAccess(ins, cs.getArgument(1), false));
				}
				if (callee->getName() == "BZ2_bzBuffToBuffDecompress") {
					result.push_back(PointerAccess(ins, cs.getArgument(0), true));
				}
			}
		}
		return result;
	}

	bool racy(const PointerAccess &a, const PointerAccess &b) {
		return a.is_write + b.is_write >= 1;
	}
}
