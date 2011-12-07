/**
 * Author: Jingyue
 */

#include "llvm/Pass.h"
using namespace llvm;

namespace slicer {
	struct IntTest: public ModulePass {
		static char ID;

		IntTest(): ModulePass(ID) {}
		virtual bool runOnModule(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;

	private:
		void setup(Module &M);

		/* These test functions give assertion failures on incorrect results. */
		void aget(Module &M);
		void aget_like(Module &M);
		void test_overwrite(Module &M);
		void test_overwrite_2(Module &M);
		void fft(Module &M);
		void fft_like(Module &M);
		void fft_common(Module &M);
		void radix(Module &M);
		void radix_like(Module &M);
		void radix_common(Module &M);
		void lu_cont(Module &M);
		void blackscholes(Module &M);
		void test_loop(Module &M);
		void test_loop_2(Module &M);
		void test_reducer(Module &M);
		void test_bound(Module &M);
		void test_thread(Module &M);
		void test_thread_2(Module &M);
		void test_array(Module &M);
		void test_malloc(Module &M);
		void test_range(Module &M);
		void test_range_2(Module &M);
		void test_range_3(Module &M);
		void test_dep(Module &M);
		void test_dep_common(Module &M);
		void test_range_4(Module &M);
		void test_ctxt_2(Module &M);
		void test_ctxt_4(Module &M);
		void test_global(Module &M);
		void test_lcssa(Module &M);
		void test_barrier(Module &M);
		void test_path_2(Module &M);
		void test_alloca(Module &M);

		void check_fake_pwrite(Module &M);
		void check_fake_pwrite_cs(Module &M);
		void check_transpose(Module &M);
		void check_fft1donce(Module &M);

		const Type *int_type;
	};
}
