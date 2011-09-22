/**
 * Author: Jingyue
 */

#ifndef __SLICER_LISTENER_H
#define __SLICER_LISTENER_H

#include "llvm/PassSupport.h"
using namespace llvm;

#include <string>
#include <map>
using namespace std;

namespace slicer {

	/**
	 * Listen to the pass registration events,
	 * and capture the Iterator pass and the BranchRemover pass. 
	 *
	 * TODO: Not necessary in LLVM 2.9. We could easily find these passes
	 * in the PassRegistry. 
	 */
	struct SimplifierListener: public PassRegistrationListener {

		SimplifierListener() {}
		virtual void passRegistered(const PassInfo *P);
		virtual void passEnumerate(const PassInfo *P) { passRegistered(P); }

		const PassInfo *getPassInfo(const string &Name) const;

	private:
		map<string, const PassInfo *> NameToPassInfo;
	};
}

#endif
