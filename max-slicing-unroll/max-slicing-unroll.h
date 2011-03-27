/**
 * Author: Jingyue
 */

#ifndef __SLICER_MAX_SLICING_UNROLL_H
#define __SLICER_MAX_SLICING_UNROLL_H

#include "llvm/Pass.h"
#include "llvm/ADT/DenseSet.h"

#include "idm/id.h"
#include "common/include/typedefs.h"

#include <vector>
#include <string>
#include <map>

using namespace std;
using namespace llvm;

namespace slicer {

	const static string SLICER_SUFFIX = ".SLICER";

	struct MaxSlicingUnroll: public ModulePass {

		enum EdgeType {
			EDGE_CALL,
			EDGE_INTER_BB,
			EDGE_INTRA_BB,
			EDGE_RET
		};

		struct ThreadCreationRecord {
			ThreadCreationRecord(int p, size_t t, int c):
				parent_tid(p), trunk_id(t), child_tid(c) {}
			int parent_tid;
			size_t trunk_id;
			int child_tid;
		};

		static char ID;

		typedef DenseMap<Instruction *, InstList> CFG;
		typedef InstPair Edge;
		typedef DenseSet<Edge> EdgeSet;
		typedef map<int, InstList> Trace;

		MaxSlicingUnroll();
		MaxSlicingUnroll(const string &trace_file, const string &cut_file);

		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual bool runOnModule(Module &M);
		virtual void print(raw_ostream &O, const Module *M) const;
		Instruction *get_cloned_inst(
				int thr_id,
				unsigned trunk_id,
				Instruction *orig) const;
		Instruction *get_orig_inst(Instruction *cloned) const;

	private:
		void dump_thr_cfg(const CFG &cfg, int thr_id);
		void link_thr_funcs(
				Module &M,
				const Trace &trace,
				const vector<ThreadCreationRecord> &thr_cr_records);
		void link_thr_func(
				Module &M,
				const Trace &trace,
				int parent_tid,
				size_t trunk_id,
				int child_tid);
		void add_cfg_edge(Instruction *x, Instruction *y);
		/*
		 * Builds the control flow graph. 
		 * Builds <clone_map> and <clone_map_r> as well. 
		 */
		void build_cfg(
				Module &M,
				const Trace &trace,
				const InstSet &cut);
		/*
		 * <cfg> and <parent> are shared by all threads.
		 * Do *not* clear them in this function. 
		 */
		void build_cfg_of_thread(
				Module &M,
				const InstList &thr_trace,
				const InstSet &cut,
				int thr_id);
		/*
		 * Build the CFG of a particular trunk. 
		 * [start, end] indicates the range of the trunk.
		 */
		void build_cfg_of_trunk(
				Instruction *start,
				Instruction *end,
				const InstSet &cut,
				int thr_id,
				size_t trunk_id,
				vector<Instruction *> &call_stack);
		/*
		 * Link the original instruction and the cloned instruction
		 * in clone mappings.
		 */
		void link_orig_cloned(
				Instruction *orig,
				Instruction *cloned,
				int thr_id,
				size_t trunk_id);
		/*
		 * DFS algorithm used in reachability analysis. 
		 * This one exploits the call stack and is different from
		 * the standard one.
		 */
		void dfs(
				Instruction *x,
				const InstSet &cut,
				InstSet &visited_nodes,
				EdgeSet &visited_edges,
				InstList &call_stack);
		/*
		 * A common function used in DFS.
		 * Modifies visited_nodes and visited_edges,
		 * and calls DFS recursively. 
		 */
		void move_on(
				Instruction *x,
				Instruction *y,
				const InstSet &cut,
				InstSet &visited_nodes,
				EdgeSet &visited_edges,
				InstList &call_stack);
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
		void assign_container(
				Module &M,
				Instruction *x,
				const DenseMap<Instruction *, int> &level,
				const InstMapping &parent);
		/*
		 * Assign call-levels to instructions in the CFG. 
		 * As a side-effect, calculate the parent of each instruction
		 * in the DFS tree. 
		 */
		void assign_level(
				Instruction *x,
				DenseMap<Instruction *, int> &level,
				InstMapping &parent);
		int read_trace_and_cut(
				const string &trace_file,
				const string &cut_file,
				Trace &trace,
				vector<ThreadCreationRecord> &thr_cr_records,
				InstSet &cut);
		int read_trace(
				const string &trace_file,
				Trace &trace,
				vector<ThreadCreationRecord> &thr_cr_records);
		int read_cut(const string &cut_file, InstSet &cut);
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
		 * We set the name for the new instruction,
		 * and resolve function-local metadata.
		 */
		Instruction *clone_inst(const Instruction *x);
		/*
		 * This function is called after dfs.
		 *
		 * dfs traverses the CFG from <start>
		 * to end without touching anything in the cut. However, it doesn't
		 * guarantee that the CFG of the trunk stops at <end>. This function
		 * refines backwards the CFG of the trunk from <end>. 
		 */
		void refine_from_end(
				Instruction *start,
				Instruction *end,
				const InstSet &cut,
				InstSet &visited_nodes,
				EdgeSet &visited_edges);
		/* For debugging */
		void print_inst_set(const InstSet &s);
		void print_edge_set(const EdgeSet &s);
		void print_cloned_inst(Instruction *ii);
		void print_levels_in_thread(
				int thr_id,
				const DenseMap<Instruction *, int> &level);
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
		void dfs_cfg(
				const CFG &cfg,
				Instruction *x,
				const InstSet &cut,
				InstMapping &parent);
		/*
		 * Traces back through <parent> and finds the latest ancestor with
		 * the same level. 
		 */
		Instruction *find_parent_at_same_level(
				Instruction *x,
				const DenseMap<Instruction *, int> &level,
				const InstMapping &parent);
		/*
		 * Fix the def-use graph. 
		 */
		void fix_def_use(
				Module &M,
				const Trace &trace);
		void fix_def_use_bb(
				Module &M);
		Instruction *find_op_in_cloned(
				Instruction *op,
				Instruction *user,
				const InstMapping &parent);
		void fix_def_use_insts(
				Module &M,
				const Trace &trace);
		void fix_def_use_func_param(Module &M);
		void fix_def_use_func_call(Module &M);
		void redirect_program_entry(Module &M, Instruction *old_start);
		void stat(Module &M);

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
	};
}

#endif

