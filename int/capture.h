#ifndef __SLICER_CAPTURE_H
#define __SLICER_CAPTURE_H

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Instructions.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/DominatorInternals.h"
#include "common/include/typedefs.h"
#include "common/cfg/icfg.h"
using namespace llvm;

#include <vector>
#include <map>
using namespace std;

#include "expression.h"

namespace slicer {

	struct CaptureConstraints: public ModulePass {

		static char ID;

#if 0
		enum EdgeType {
			EDGE_CALL,
			EDGE_INTER_BB,
			EDGE_INTRA_BB,
			EDGE_RET
		};
#endif

		const static unsigned INVALID_VAR_ID = (unsigned)-1;

		typedef DenseMap<Value *, pair<Expr, Expr> > ValueBoundsInBB;
		typedef pair<Value *, pair<Expr, Expr> > BoundsInBB;

		CaptureConstraints();
		virtual ~CaptureConstraints();
		virtual bool runOnModule(Module &M);
		bool recalculate(Module &M);
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual void print(raw_ostream &O, const Module *M) const;

		void replace_aa(AliasAnalysis *new_AA);

		unsigned get_num_constraints() const;
		const Clause *get_constraint(unsigned i) const;
		/**
		 * Compute a finger print of all the constraints captured. 
		 * Used to check whether the iterative process should stop. 
		 */
		long get_fingerprint() const;
		/**
		 * In <integers>?
		 */
		bool is_integer(const Value *v) const {
			return integers.count(const_cast<Value *>(v));
		}
		const ConstValueSet &get_integers() const { return integers; }
		/**
		 * Used by the solver as well, so need make it public.
		 * Called internally by the module to capture constraints on
		 * unreachable BBs. 
		 */
		Clause *get_avoid_branch(const TerminatorInst *ti, unsigned i) const;

	private:
		// Utility functions. 
		static void print_value(raw_ostream &O, const Value *v);
		static Value *get_pointer_operand(Instruction *i);
		static const Value *get_pointer_operand(const Instruction *i);
		static Value *get_value_operand(Instruction *i);
		static const Value *get_value_operand(const Instruction *i);
		
		// Address taken variables. 
		void capture_addr_taken(Module &M);
		/*
		 * store v1, p (including global variable initializers)
		 * v2 = load q
		 * p and q may alias
		 * =>
		 * v2 may = v1
		 */
		void capture_may_assign(Module &M);
		void capture_must_assign(Module &M);
		void capture_overwriting_to(LoadInst *i2);
		Instruction *find_latest_overwriter(Instruction *i2, Value *q);
		BasicBlock *get_idom(BasicBlock *bb);
		Instruction *get_idom(Instruction *ins);
		MicroBasicBlock *get_idom_ip(MicroBasicBlock *mbb);
		Instruction *get_idom_ip(Instruction *ins);
		Instruction *find_nearest_common_dom(Instruction *i1, Instruction *i2);
		// Check if any instruction between <i1> and <i2> may write to <q>. 
		// <i1> must dominate <i2>, and they are in the same function. 
		// We don't consider <i1> and <i2> in the path, i.e. the path is an
		// exclusive region (i1, i2). 
		bool path_may_write(
				const Instruction *i1, const Instruction *i2, const Value *q);
		// Check if instruction <i> may write to <q>. 
		bool may_write(
				const Instruction *i, const Value *q, ConstFuncSet &visited_funcs);
		bool may_write(
				const Function *f, const Value *q, ConstFuncSet &visited_funcs);
		
		// General functions. 
		void simplify_constraints();
		void stat(Module &M);
		void setup(Module &M);

		// Integer and pointer values. 
		void capture_top_level(Module &M);
		void identify_integers(Module &M);
		/* 
		 * Extract constants from constant <c>.
		 * <c> might be a constant expression, so we need extract constants
		 * recursively.
		 */
		void extract_from_consts(Constant *c);
		void add_eq_constraint(const Value *v1, const Value *v2);
		// TODO: We care about Instructions and ConstantExprs only. We could use
		// Operator as the argument. 
		void capture_in_user(const User *user);
		/*
		 * These capture_* functions need to check whether their operands
		 * are constant.
		 */
		void capture_in_icmp(const ICmpInst *user);
		void capture_in_unary(const User *user);
		void capture_in_binary(const User *user, unsigned opcode);
		void capture_in_gep(const User *user);
		void capture_in_phi(const PHINode *phi);
		/* Constraints from unreachable blocks. */
		void capture_unreachable(Module &M);
		void capture_unreachable_in_func(Function *f);
		/* Function summaries. */
		void capture_func_summaries(Module &M);
		void capture_libcall(const CallSite &cs);

		// Data members. 
		vector<Clause *> constraints;
		ConstValueSet integers;
		const Type *int_type;
		AliasAnalysis *AA;
		DominatorTreeBase<ICFGNode> IDT;
	};
}

#endif
