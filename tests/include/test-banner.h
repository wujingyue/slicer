#ifndef __SLICER_TEST_BANNER_H
#define __SLICER_TEST_BANNER_H

#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#include <string>
using namespace std;

namespace slicer {

	struct TestBanner {

		static void print_banner(const string &content) {
			errs() << "+" << string(content.length() + 2, '-') << "+\n";
			errs() << "| " << content << " |\n";
			errs() << "+" << string(content.length() + 2, '-') << "+\n";
		}

		TestBanner(const string &name) {
			this->name = name;
			print_banner("Running Test " + name);
		}

		~TestBanner() {
			print_banner("Finished Test " + name);
		}

	private:
		string name;
	};
}

#endif
