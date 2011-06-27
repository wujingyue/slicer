#ifndef __SLICER_TEST_BANNER_H
#define __SLICER_TEST_BANNER_H

#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#include <string>
using namespace std;

namespace slicer {

	struct TestBanner {

		TestBanner(const string &name) {
			this->name = name;
			errs() << "===== Running Test " << name << " =====\n";
		}
		~TestBanner() {
			errs() << "===== Finished Test " << name << " =====\n";
		}

	private:
		string name;
	};
}

#endif
