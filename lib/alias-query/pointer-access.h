/**
 * Author: Jingyue
 */

#ifndef __SLICER_POINTER_ACCESS_H
#define __SLICER_POINTER_ACCESS_H

#include <vector>
using namespace std;

#include "llvm/Value.h"
using namespace llvm;

namespace slicer {
	struct PointerAccess {
		PointerAccess(const Instruction *acc, const Value *p, bool write);

		const Instruction *accessor;
		const Value *loc;
		bool is_write;
	};

	vector<PointerAccess> get_pointer_accesses(const Instruction *ins);
	/**
	 * Only checks whether at least one of them is a write. 
	 * It doesn't check whether they may run concurrently. 
	 */
	bool racy(const PointerAccess &a, const PointerAccess &b);
}

#endif
