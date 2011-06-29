#ifndef __SLICER_LISTENER_H
#define __SLICER_LISTENER_H

#include "llvm/PassSupport.h"
using namespace llvm;

namespace slicer {

	/**
	 * Listen to the pass registration events,
	 * and capture the Iterator pass and the BranchRemover pass. 
	 *
	 * TODO: Not necessary in LLVM 2.9. We could easily find these passes
	 * in the PassRegistry. 
	 */
	struct SimplifierListener: public PassRegistrationListener {

		SimplifierListener():
			Iterator(NULL), BranchRemover(NULL) {}
		virtual void passRegistered(const PassInfo *P);
		virtual void passEnumerate(const PassInfo *P) { passRegistered(P); }

		const PassInfo *getIterator() const { return Iterator; }
		const PassInfo *getBranchRemover() const { return BranchRemover; }

	private:
		const PassInfo *Iterator, *BranchRemover;
	};
}

#endif
