/**
 * Author: Jingyue
 */

#include "llvm/Pass.h"
using namespace llvm;

namespace slicer {
	struct IntTest: public ModulePass {
		static char ID;

		IntTest(): ModulePass(&ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;

	private:
		/* These test functions give assertion failures on incorrect results. */
		void test_aget(const Module &M);
		void test_aget_like(const Module &M);
		void test_test_overwrite(const Module &M);
		void test_test_overwrite_2(const Module &M);
		void test_fft(const Module &M);
		void test_fft_like(const Module &M);
		void test_fft_tern(const Module &M);
		void test_fft_tern_2(const Module &M);
		void test_fft_common(const Module &M);
		void test_radix(const Module &M);
		void test_radix_like(const Module &M);
		void test_radix_common(const Module &M);
		void test_test_loop(const Module &M);
		void test_test_loop_2(const Module &M);
		void test_test_reducer(const Module &M);
		void test_test_bound(const Module &M);
		void test_test_thread(const Module &M);
		void test_test_thread_2(const Module &M);
		void test_test_array(const Module &M);
		void test_test_malloc(const Module &M);
		void test_test_range(const Module &M);
		void test_test_range_2(const Module &M);
		void test_test_range_3(const Module &M);
		void test_test_dep(const Module &M);
		void test_test_dep_common(const Module &M);
		void test_test_range_4(const Module &M);
		void test_test_ctxt_2(const Module &M);
		void test_test_ctxt_4(const Module &M);
		void test_test_global(const Module &M);
	};
}
