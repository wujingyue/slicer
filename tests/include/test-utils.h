#ifndef __SLICER_TEST_UTILS_H
#define __SLICER_TEST_UTILS_H

#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#include <string>
using namespace std;

namespace slicer {

	struct TestBanner {

		static void print_banner(const string &content) {
			errs().changeColor(raw_ostream::BLUE, true);
			errs() << "+" << string(content.length() + 2, '-') << "+\n";
			errs() << "| " << content << " |\n";
			errs() << "+" << string(content.length() + 2, '-') << "+\n";
			errs().resetColor();
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

	static inline void print_pass(raw_ostream &O) {
		O.changeColor(raw_ostream::GREEN, true) << "Passed\n";
		O.resetColor();
	}

	static inline void print_fail(raw_ostream &O) {
		O.changeColor(raw_ostream::RED, true) << "Failed\n";
		O.resetColor();
	}

	static inline void print_result(raw_ostream &O, bool result) {
		if (result)
			print_pass(O);
		else
			print_fail(O);
	}
}

#endif
