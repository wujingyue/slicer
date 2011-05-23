/**
 * Author: Jingyue
 */

#include "llvm/Module.h"
#include "llvm/LLVMContext.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

#include "config.h"
#include "max-slicing-unroll.h"
#include "idm/id.h"
#include "common/callgraph-fp/callgraph-fp.h"
#include "common/may-exec/may-exec.h"

#include <iostream>
#include <fstream>
#include <sstream>

using namespace llvm;
using namespace std;

namespace slicer {

	void MaxSlicingUnroll::add_cfg_edge(
			Instruction *x,
			Instruction *y) {
		assert(clone_map_r.count(x) && "<x> must be in the cloned CFG");
		assert(clone_map_r.count(y) && "<y> must be in the cloned CFG");
		cfg[x].push_back(y);
		cfg_r[y].push_back(x);
	}

	void MaxSlicingUnroll::build_cfg(
			Module &M,
			const Trace &trace,
			const InstSet &cut) {

		cerr << "\nBuilding CFG...\n";
		// Initialize global variables related to building CFG. 
		cfg.clear();
		cfg_r.clear();
		clone_map.clear();
		clone_map_r.clear();
		cloned_to_trunk.clear();
		cloned_to_tid.clear();

		forallconst(Trace, it, trace) {
			// it->first: thread ID
			// it->second: thread trace
			build_cfg_of_thread(
					M,
					it->second,
					cut,
					it->first);
		}
#ifdef CHECK
		// Every instructions in the cloned program should have parent BBs and
		// parent functions.
		forall(InstMapping, it, clone_map_r) {
			assert(it->first->getParent() &&
					"Each instruction should have its parent BB");
			assert(it->first->getParent()->getParent() &&
					"Each instruction should have its parent function");
		}
#endif
		cerr << "Done building CFG\n";
	}

	void MaxSlicingUnroll::dump_thr_cfg(const CFG &cfg, int thr_id) {
		cerr << "Printing CFG of Thread " << thr_id << "...\n";
		ObjectID &IDM = getAnalysis<ObjectID>();
		for (size_t i = 0; i < clone_map[thr_id].size(); ++i) {
			forall(InstMapping, it, clone_map[thr_id][i]) {
				Instruction *orig = it->first;
				Instruction *cloned = it->second;
				const InstList &next_insts = cfg.lookup(cloned);
				for (size_t j = 0; j < next_insts.size(); ++j) {
					Instruction *cloned_next = next_insts[j];
					Instruction *orig_next = clone_map_r[cloned_next];
					assert(orig_next);
					assert(cloned_to_trunk.count(cloned) &&
							cloned_to_trunk.count(cloned_next));
					cerr << "{" << cloned_to_trunk.lookup(cloned)
						<< ":" << IDM.getInstructionID(orig)
						<< "} => {" << cloned_to_trunk.lookup(cloned_next)
						<< ":" << IDM.getInstructionID(orig_next)
						<< "}\n";
				}
			}
		}
	}

	Instruction *MaxSlicingUnroll::find_parent_at_same_level(
			Instruction *x,
			const DenseMap<Instruction *, int> &level,
			const InstMapping &parent) {
		/*
		 * This piece of code can be simplified using only one for-loop.
		 * But we don't do that because we need those assertions.
		 */
		assert(level.count(x));
		int level_of_x = level.lookup(x);
		Instruction *p = x;
		do {
			Instruction *old_p = p;
			p = parent.lookup(p);
			assert(p);
			assert(p != old_p);
			assert(level.count(p));
		} while (level.lookup(p) != level_of_x);

		return p;
	}

	void MaxSlicingUnroll::assign_level(
			Instruction *x,
			DenseMap<Instruction *, int> &level,
			InstMapping &parent) {
		assert(x && "<x> cannot be NULL");
		const InstList &next_insts = cfg.lookup(x);
		for (size_t j = 0, E = next_insts.size(); j < E; ++j) {
			Instruction *y = next_insts[j];
			if (!level.count(y)) {
				EdgeType t = get_edge_type(x, y);
				if (t == EDGE_CALL)
					level[y] = level[x] + 1;
				else if (t == EDGE_RET)
					level[y] = level[x] - 1;
				else
					level[y] = level[x];
				parent[y] = x;
				assign_level(y, level, parent);
			}
		}
	}

	void MaxSlicingUnroll::assign_containers(
			Module &M,
			Instruction *start) {
		DenseMap<Instruction *, int> level;
		level[start] = 0;
		InstMapping parent;
		parent[start] = start;
		assign_level(start, level, parent);
		// DFS from the start. Will traverse all instructions in the CFG. 
		assign_container(M, start, level, parent);
	}

	void MaxSlicingUnroll::assign_container(
			Module &M,
			Instruction *x,
			const DenseMap<Instruction *, int> &level,
			const InstMapping &parent) {
		assert(x && "<x> cannot be NULL");
		if (x->getParent())
			return;

		Instruction *old_x = clone_map_r.lookup(x);
		assert(old_x && "<x> must appear in the reverse clone map");
		BasicBlock *old_bb = old_x->getParent();
		Function *old_func = old_bb->getParent();
		
		// If <old_x> is the entry of a BB, we need create a new BB for <x>;
		// otherwise, append <x> to the BB containing its previous instruction. 
		// If <old_x> is also the entry of a function, create a new function. 
		BasicBlock *bb = NULL;
		if (old_x == old_bb->begin()) {
			Function *func;
			if (old_bb == old_func->begin()) {
				func = Function::Create(
						old_func->getFunctionType(),
						old_func->getLinkage(),
						old_func->getNameStr() + SLICER_SUFFIX,
						&M);
				Function::arg_iterator ai = func->arg_begin();
				Function::arg_iterator old_ai = old_func->arg_begin();
				for (; ai != func->arg_end() && old_ai != old_func->arg_end();
						++ai, ++old_ai)
					ai->setName(old_ai->getName());
			} else {
				Instruction *p = find_parent_at_same_level(x, level, parent);
				func = p->getParent()->getParent();
			}
			bb = BasicBlock::Create(getGlobalContext(), "bb", func);
		} else {
			Instruction *p = find_parent_at_same_level(x, level, parent);
			bb = p->getParent();
		}
		assert(bb);
		// Append <x> to <bb>.
		bb->getInstList().push_back(x);
		assert(x->getParent());
		assert(x->getParent()->getParent());

		// Continue DFSing.
		const InstList &next_insts = cfg.lookup(x);
		for (size_t j = 0, E = next_insts.size(); j < E; ++j) {
			Instruction *y = next_insts[j];
			assign_container(M, y, level, parent);
		}
	}

	MaxSlicingUnroll::EdgeType MaxSlicingUnroll::get_edge_type(
			Instruction *x,
			Instruction *y) {
		assert(x && y && "<x> and <y> cannot be NULL");
		x = clone_map_r.lookup(x);
		assert(x && "<x> must be in the cloned CFG");
		y = clone_map_r.lookup(y);
		assert(y && "<y> must be in the cloned CFG");

		if (x->getParent()->getParent() != y->getParent()->getParent()) {
			// In different functions. Either call or ret. 
			if (is_call(x))
				return EDGE_CALL;
			else {
				assert((isa<ReturnInst>(x) || isa<UnwindInst>(x)) &&
						"<x> should be either a call or a ret");
				return EDGE_RET;
			}
		} else if (x->getParent() == y->getParent())
			return EDGE_INTRA_BB;
		else
			return EDGE_INTER_BB;
	}

	Instruction *MaxSlicingUnroll::clone_inst(const Instruction *x) {
		Instruction *y = x->clone();
		y->setName(x->getName());
		// Some operands need to be cloned as well, e.g. function-local
		// metadata. Here we reuse the MapValue function which is used
		// in RemapInstruction. But we pass an empty value mapping so that
		// it won't map any instruction operand for now. 
		DenseMap<const Value *, Value *> empty_value_map;
		for (unsigned i = 0; i < y->getNumOperands(); ++i) {
			Value *op = y->getOperand(i);
			empty_value_map.clear();
			Value *new_op = MapValue(op, empty_value_map);
			if (new_op)
				y->setOperand(i, new_op);
		}
		return y;
	}

	void MaxSlicingUnroll::build_cfg_of_thread(
			Module &M,
			const InstList &thr_trace,
			const InstSet &cut,
			int thr_id) {
		cerr << "Building CFG of Thread " << thr_id << "...\n";
		clone_map[thr_id].clear();
		vector<Instruction *> call_stack;
		// Iterate through each trunk
		for (size_t i = 0, E = thr_trace.size(); i + 1 < E; ++i) {
			cerr << "Building CFG of Trunk " << i << "...\n";
			build_cfg_of_trunk(
					thr_trace[i],
					thr_trace[i + 1],
					cut,
					thr_id,
					i,
					call_stack);
		}

#ifdef VERBOSE
		dump_thr_cfg(cfg, thr_id);
#endif
		// Assign containers. 
		assert(!thr_trace.empty());
		// TODO: It's possible that the CFG of a thread is incomplete
		// due to not exiting normally. We need to detect incomplete functions
		// and delete them. 
		if (!clone_map[thr_id].empty()) {
			Instruction *start = clone_map[thr_id][0].lookup(thr_trace[0]);
			assert(start);
			assign_containers(M, start);
		}
	}

	void MaxSlicingUnroll::build_cfg_of_trunk(
			Instruction *start,
			Instruction *end,
			const InstSet &cut,
			int thr_id,
			size_t trunk_id,
			vector<Instruction *> &call_stack) {
		// DFS in both directions to find the instructions may be
		// visited in the trunk. 
		InstSet visited_nodes; visited_nodes.insert(start);
		EdgeSet visited_edges;
		dfs(start, cut, visited_nodes, visited_edges, call_stack);
#ifdef VERBOSE
		if (!visited_nodes.count(end)) {
			start->dump();
			end->dump();
		}
#endif
		assert(visited_nodes.count(end) &&
				"Unable to reach from <start> to <end>");
		refine_from_end(start, end, cut, visited_nodes, visited_edges);
		// Clone instructions in this trunk. 
		// Note <start> may equal <end>. 
		clone_map[thr_id].push_back(InstMapping());
		forall(InstSet, it, visited_nodes) {
			Instruction *orig = *it;
			// <start> should be already cloned in the last trunk
			// except for the first trunk. 
			// <end> should be cloned into the next trunk. 
			if (orig != end && (orig != start || trunk_id == 0))
				link_orig_cloned(orig, clone_inst(orig), thr_id, trunk_id);
		}
		link_orig_cloned(end, clone_inst(end), thr_id, trunk_id + 1);
#ifdef VERBOSE
		print_inst_set(visited_nodes);
		print_edge_set(visited_edges);
#endif
		// Add this trunk to the CFG. 
		forall(EdgeSet, it, visited_edges) {
			Instruction *x, *y, *x1, *y1;
			x = it->first;
			y = it->second;
			x1 = clone_map[thr_id][trunk_id].lookup(x);
			if (!x1)
				x->dump();
			assert(x1);
			if (y == end)
				y1 = clone_map[thr_id][trunk_id + 1].lookup(y);
			else
				y1 = clone_map[thr_id][trunk_id].lookup(y);
			assert(y1);
			add_cfg_edge(x1, y1);
		}
	}

	void MaxSlicingUnroll::link_orig_cloned(
			Instruction *orig,
			Instruction *cloned,
			int thr_id,
			size_t trunk_id) {
		while (trunk_id >= clone_map[thr_id].size())
			clone_map[thr_id].push_back(InstMapping());
		clone_map[thr_id][trunk_id][orig] = cloned;
		clone_map_r[cloned] = orig;
		cloned_to_trunk[cloned] = trunk_id;
		cloned_to_tid[cloned] = thr_id;
	}

	void MaxSlicingUnroll::refine_from_end(
			Instruction *start,
			Instruction *end,
			const InstSet &cut,
			InstSet &visited_nodes,
			EdgeSet &visited_edges) {
		// A temporary reverse CFG. 
		CFG cfg_t;
		forall(EdgeSet, it, visited_edges) {
			Instruction *x = it->first, *y = it->second;
			cfg_t[y].push_back(x);
		}
		// DFS from the end.
		InstMapping parent;
		parent[end] = end;
		dfs_cfg(cfg_t, end, cut, parent);
		// Refine the visited_nodes and the visited_edges so that
		// only nodes that can be visited in both directions are
		// taken. 
		InstSet orig_visited_nodes(visited_nodes);
		EdgeSet orig_visited_edges(visited_edges);
		visited_nodes.clear();
		visited_edges.clear();
		forall(InstSet, it, orig_visited_nodes) {
			if (parent.count(*it))
				visited_nodes.insert(*it);
		}
		forall(EdgeSet, it, orig_visited_edges) {
			if (parent.count(it->first) && parent.count(it->second))
				visited_edges.insert(*it);
		}
	}
	
	void MaxSlicingUnroll::dfs_cfg(
			const CFG &cfg,
			Instruction *x,
			const InstSet &cut,
			InstMapping &parent) {
		assert(x && "<x> cannot be NULL");
		assert(parent.count(x) && "<x>'s parent hasn't been set");
		const InstList &next_insts = cfg.lookup(x);
		for (size_t j = 0, E = next_insts.size(); j < E; ++j) {
			Instruction *y = next_insts[j];
			if (parent.lookup(y) == NULL) {
				// Has not visited <y>. 
				parent[y] = x;
				if (!cut.count(y))
					dfs_cfg(cfg, y, cut, parent);
			}
		}
	}

	void MaxSlicingUnroll::dfs(
			Instruction *x,
			const InstSet &cut,
			InstSet &visited_nodes,
			EdgeSet &visited_edges,
			InstList &call_stack) {

		assert(x && "<x> cannot be NULL");
		assert(visited_nodes.count(x));

#ifdef VERBOSE
		cerr << "dfs:";
		x->dump();
#endif

		if (is_call(x) && !is_intrinsic_call(x)) {
			bool may_exec_landmark = false;
			CallGraphFP &CG = getAnalysis<CallGraphFP>();
			MayExec &ME = getAnalysis<MayExec>();
			const FuncList &callees = CG.get_called_functions(x);
			for (size_t j = 0, E = callees.size(); j < E; ++j) {
				if (ME.may_exec_landmark(callees[j]))
					may_exec_landmark = true;
			}
			// If no callees may execute landmarks, we don't dive into the callee. 
			// Instead, we simply call the original function. 
			if (may_exec_landmark) {
				for (size_t j = 0, E = callees.size(); j < E; ++j) {
					if (callees[j]->isDeclaration())
						continue;
					Instruction *y = callees[j]->getEntryBlock().begin();
					call_stack.push_back(x);
					move_on(x, y, cut, visited_nodes, visited_edges, call_stack);
				}
				return;
			}
			// If the callee cannot execute any landmark,
			// treat it as a regular instruction without
			// going into the function body. 
		} // if is_call

		if (isa<ReturnInst>(x) || isa<UnwindInst>(x)) {
			assert(!call_stack.empty());
			BasicBlock::iterator ret_addr = call_stack.back();
			BasicBlock::iterator y;
			if (isa<CallInst>(ret_addr)) {
				y = ret_addr; ++y;
			} else if (isa<InvokeInst>(ret_addr)) {
				InvokeInst *inv = dyn_cast<InvokeInst>(ret_addr);
				if (isa<ReturnInst>(x))
					y = inv->getNormalDest()->begin();
				else
					y = inv->getUnwindDest()->begin();
			} else {
				assert(false && "Only CallInsts and InvokeInsts can call functions");
			}
			call_stack.pop_back();
			move_on(x, y, cut, visited_nodes, visited_edges, call_stack);
			return;
		}

		if (!x->isTerminator()) {
			BasicBlock::iterator y = x; ++y;
			move_on(x, y, cut, visited_nodes, visited_edges, call_stack);
		} else {
			TerminatorInst *ti = dyn_cast<TerminatorInst>(x);
			for (unsigned j = 0, E = ti->getNumSuccessors(); j < E; ++j) {
				Instruction *y = ti->getSuccessor(j)->begin();
				move_on(x, y, cut, visited_nodes, visited_edges, call_stack);
			}
		}
	}

	void MaxSlicingUnroll::move_on(
			Instruction *x,
			Instruction *y,
			const InstSet &cut,
			InstSet &visited_nodes,
			EdgeSet &visited_edges,
			InstList &call_stack) {
#ifdef VERBOSE
		cerr << "move_on:";
		y->dump();
#endif
		/*
		 * No matter whether <y> is in the cut, we mark <y> and <x, y>
		 * as visited. But we don't continue traversing <y> if <y>
		 * is in the cut.
		 */
		visited_edges.insert(make_pair(x, y));
		if (!visited_nodes.count(y)) {
			visited_nodes.insert(y);
			if (!cut.count(y))
				dfs(y, cut, visited_nodes, visited_edges, call_stack);
		}
	}

}

