/**
 * Author: Jingyue
 */

/**
 * TODO: redirect_program_entry does not remove any instructions now. 
 * Therefore, we can print the clone mapping in an much easier way. 
 *
 * TODO: CFG in the MBB-level. 
 */

#ifndef __SLICER_MAX_SLICING_H
#define __SLICER_MAX_SLICING_H

#include "llvm/Pass.h"
#include "llvm/ADT/DenseSet.h"
#include "common/typedefs.h"
using namespace llvm;

#include <vector>
#include <string>
#include <map>
using namespace std;

namespace slicer {
	static const string SLICER_SUFFIX = ".SLICER";
	static const string OLDMAIN_SUFFIX = ".OLDMAIN";

	struct MaxSlicing: public ModulePass {
		enum EdgeType {
			EDGE_CALL,
			EDGE_INTER_BB,
			EDGE_INTRA_BB,
			EDGE_RET
		};

		// TODO: in MBB-level.
		typedef DenseMap<Instruction *, InstList> CFG;
		typedef InstPair Edge;
		typedef DenseSet<Edge> EdgeSet;
		typedef map<int, InstList> Trace;

		static char ID;
		MaxSlicing();
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual bool runOnModule(Module &M);

		/**
		 * Returns whether <bb> is one of the unreachable BBs we added. 
		 */
		static bool is_unreachable(const BasicBlock *bb);
		/**
		 * Returns whether the function is in the sliced part. 
		 */
		static bool is_sliced(const Function *f);
		/**
		 * Create an unreachable BB.
		 * Used by MaxSlicing itself, and Reducer. 
		 */
		static BasicBlock *create_unreachable(Function *f);

	private:
		/**
		 * <trace> and <landmarks> will be updated. 
		 */
		void read_trace_and_landmarks();
		void dump_thr_cfg(const CFG &cfg, int thr_id);
		void link_thr_funcs(Module &M);
		void link_thr_func(Module &M,
				int parent_tid, size_t trunk_id, int child_tid);
		void compute_reachability(Module &M);
		void compute_reachability(Function *f);
		void add_cfg_edge(Instruction *x, Instruction *y);
		/*
		 * Builds the control flow graph. 
		 * Builds <clone_map> and <clone_map_r> as well. 
		 */
		void build_cfg(Module &M);
		/*
		 * <cfg> and <parent> are shared by all threads.
		 * Do *not* clear them in this function. 
		 */
		void build_cfg_of_thread(Module &M, int thr_id);
		/**
		 * Build the CFG of a particular trunk. 
		 * [start, end] indicates the range of the trunk.
		 *
		 * <call_stack> should contain the calling context of <start>
		 * when calling this function. It will contain the calling context
		 * of <end> after this function returns. 
		 */
		void build_cfg_of_trunk(Instruction *start, Instruction *end,
				int thr_id, size_t trunk_id, InstList &call_stack);
		/*
		 * Create the cloned instruction, and
		 * link the original instruction and the cloned instruction
		 * in clone mappings.
		 */
		void create_and_link_cloned_inst(
				int thr_id, size_t trunk_id, Instruction *orig);
		/**
		 * DFS algorithm used in reachability analysis. 
		 * This one exploits the call stack and is different from
		 * the standard one.
		 *
		 * <end_call_stack> records the calling context when reaching <end>. 
		 */
		void dfs(Instruction *x, Instruction *end,
				InstSet &visited_nodes, EdgeSet &visited_edges,
				InstList &call_stack, InstList &end_call_stack);
		/*
		 * A common function used in DFS.
		 * Modifies visited_nodes and visited_edges,
		 * and calls DFS recursively. 
		 */
		void move_on(Instruction *x, Instruction *y,
				Instruction *end,
				InstSet &visited_nodes, EdgeSet &visited_edges,
				InstList &call_stack, InstList &end_call_stack);
		/*
		 * Assign each instruction a containing BB and a containing function. 
		 * We calculate the assignment of an instruction according to its
		 * preceding instructions in <cfg>.
		 *
		 * This function needs to DFS the CFG. <start> indicates the start point,
		 * which should be the start point of a thread, i.e. the main entry or 
		 * the entry to a thread function.
		 */
		void assign_containers(Module &M, Instruction *x);
		void assign_container(Module &M, Instruction *x,
				const DenseMap<Instruction *, int> &level, const InstMapping &parent);
		/**
		 * Compute <y>'s call-level according to its parent <x>.
		 */
		void assign_level(Instruction *y, Instruction *x,
				DenseMap<Instruction *, int> &level);
		/*
		 * Get the type of the edge from <x> to <y>. 
		 * <x> and <y> must be in the cloned CFG. 
		 * This function maps <x> and <y> back to their counterparts in the
		 * original CFG, and determine the edge type.
		 * Therefore, do not use this function before <clone_map_r> is ready. 
		 */
		EdgeType get_edge_type(Instruction *x, Instruction *y);
		/*
		 * More than x->clone(). 
		 * We set the name for the new instruction, resolve function-local
		 * metadata, and set the clone_info metadata for the new instruction. 
		 */
		Instruction *clone_inst(int thr_id, size_t trunk_id, const Instruction *x);
		/*
		 * This function is called after dfs.
		 *
		 * dfs traverses the CFG from <start>
		 * to end without touching anything in the cut. However, it doesn't
		 * guarantee that the CFG of the trunk stops at <end>. This function
		 * refines backwards the CFG of the trunk from <end>. 
		 */
		void refine_from_end(Instruction *start, Instruction *end,
				InstSet &visited_nodes, EdgeSet &visited_edges);
		/* For debugging */
		void print_inst_set(raw_ostream &O, const InstSet &s);
		void print_edge_set(raw_ostream &O, const EdgeSet &s);
		void print_call_stack(raw_ostream &O, const InstList &cs);
		/*
		 * DFS the CFG. 
		 *
		 * <parent> contains <x>'s parent in the DFS tree. 
		 * parent[start] = start.
		 * <parent> indicates which instructions have been visited as well. 
		 * 
		 * The parameter <cfg> is necessary because it's not always used
		 * for the global CFG. 
		 *
		 * Stop DFSing at <cut>. 
		 */
		void dfs_cfg(const CFG &cfg, Instruction *x,
				InstSet &visited_nodes, EdgeSet &visited_edges);
		/**
		 * Traces back through <parent> and finds the latest ancestor with
		 * the same level. 
		 */
		Instruction *find_parent_at_same_level(Instruction *x,
				const DenseMap<Instruction *, int> &level,
				const InstMapping &parent);
		/**
		 * An InvokeInst's successors may not be its successors in <cfg>. 
		 * Need an extra pass of DFS to resolve them. 
		 */
		void find_invoke_successors(Module &M);
		void find_invoke_successors_from(Module &M, Instruction *start);
		/**
		 * Fix the def-use graph. 
		 */
		void fix_def_use(Module &M);
		void fix_def_use_bb(Module &M);
		void fix_def_use_phinodes(BasicBlock *bb);
		// <unreachable_bb> is the unreachable BB in <bb>'s containing function. 
		void fix_def_use_terminator(BasicBlock *bb, BasicBlock *&unreachable_bb);
		void fix_def_use_insts(Module &M);
		void fix_def_use_insts(Function &F);
		void fix_def_use_func_param(Module &M);
		void fix_def_use_func_call(Module &M);
		void redirect_program_entry(Instruction *old_start, Instruction *new_start);

		/* Misc */
		void volatile_landmarks(Module &M);
		void check_dominance(Module &M);

		// Maps from a cloned instruction to the original instruction. 
		InstMapping clone_map_r;
		// The map between each cloned instruction to the trunk ID. 
		DenseMap<Instruction *, size_t> cloned_to_trunk;
		DenseMap<Instruction *, int> cloned_to_tid;
		// An original instruction can be mapped to multiple instructions in
		// the cloned program. However, there can be at most one of them in each
		// trunk. Therefore, each trunk has a clone map.
		map<int, vector<InstMapping> > clone_map;
		// CFG and reversed CFG
		CFG cfg, cfg_r;
		// The real successors of an InvokeInst, which are at the same level.
		// Does not include InvokeInsts whose successors in <cfg> are already
		// real successors. 
		CFG invoke_successors, invoke_predecessors;
		Trace trace; /// The entire landmark trace. 
		InstSet landmarks; /// All landmarks. 
		/**
		 * <reach_start> indicates whether the control flow is able to reach
		 * this instruction from the entry of its containing function without
		 * touching any landmark. 
		 *
		 * <reach_end> has a similar meaning. 
		 */
		ConstInstSet reach_start, reach_end;
	};
}

#endif
