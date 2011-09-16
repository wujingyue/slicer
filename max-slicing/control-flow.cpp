/**
 * Author: Jingyue
 */

#define DEBUG_TYPE "max-slicing"

#include "llvm/Module.h"
#include "llvm/LLVMContext.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "common/callgraph-fp.h"
#include "common/exec.h"
#include "common/reach.h"
#include "common/IDManager.h"
using namespace llvm;

#include <iostream>
#include <fstream>
#include <sstream>
#include <queue>
using namespace std;

#include "max-slicing.h"
#include "trace/landmark-trace.h"
using namespace slicer;

void MaxSlicing::add_cfg_edge(Instruction *x, Instruction *y) {
	assert(clone_map_r.count(x) && "<x> must be in the cloned CFG");
	assert(clone_map_r.count(y) && "<y> must be in the cloned CFG");
	cfg[x].push_back(y);
	cfg_r[y].push_back(x);
}

void MaxSlicing::compute_reachability(Function *f) {
	Exec &EXE = getAnalysis<Exec>();

	ConstBBSet sink;
	for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
		if (EXE.must_exec_landmark(bb))
			sink.insert(bb);
	}

	Reach<BasicBlock> R;
	ConstBBSet visited;
	R.floodfill(f->begin(), sink, visited);
	for (ConstBBSet::iterator it = visited.begin(); it != visited.end(); ++it) {
		const BasicBlock *bb = *it;
		for (BasicBlock::const_iterator ins = bb->begin(); ins != bb->end(); ++ins) {
			reach_start.insert(ins);
			// Even if <ins> is a landmark, <ins> can still be in <reach_start>.
			if (EXE.must_exec_landmark(ins))
				break;
		}
	}

	ConstBBSet srcs;
	for (Function::iterator bb = f->begin(); bb != f->end(); ++bb) {
		if (is_ret(bb->getTerminator()))
			srcs.insert(bb);
	}
	visited.clear();
	R.floodfill_r(srcs, sink, visited);

	for (ConstBBSet::iterator it = visited.begin(); it != visited.end(); ++it) {
		const BasicBlock *bb = *it;
		for (BasicBlock::const_iterator ins = bb->end(); ins != bb->begin(); ) {
			--ins;
			reach_end.insert(ins);
			// Even if <ins> is a landmark, <ins> can still be in <reach_end>.
			if (EXE.must_exec_landmark(ins))
				break;
		}
	}
}

void MaxSlicing::compute_reachability(Module &M) {
	for (Module::iterator f = M.begin(); f != M.end(); ++f) {
		if (!f->isDeclaration())
			compute_reachability(f);
	}
}

void MaxSlicing::build_cfg(Module &M) {
	dbgs() << "\nBuilding CFG...\n";

	// Initialize global variables related to building CFG. 
	cfg.clear();
	cfg_r.clear();
	clone_map.clear();
	clone_map_r.clear();
	cloned_to_trunk.clear();
	cloned_to_tid.clear();

#if 0
	// Compute <reach_start> and <reach_end>.
	compute_reachability(M);
#endif

	forallconst(Trace, it, trace) {
		// it->first: thread ID
		build_cfg_of_thread(M, it->first);
	}
	// Every instructions in the cloned program should have parent BBs and
	// parent functions.
	forall(InstMapping, it, clone_map_r) {
		assert(it->first->getParent() &&
				"Each instruction should have its parent BB");
		assert(it->first->getParent()->getParent() &&
				"Each instruction should have its parent function");
	}
	dbgs() << "Done building CFG\n";
}

void MaxSlicing::dump_thr_cfg(const CFG &cfg, int thr_id) {
	dbgs() << "Printing CFG of Thread " << thr_id << "...\n";
	IDManager &IDM = getAnalysis<IDManager>();
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
				dbgs() << "{" << cloned_to_trunk.lookup(cloned)
					<< ":" << IDM.getInstructionID(orig)
					<< "} => {" << cloned_to_trunk.lookup(cloned_next)
					<< ":" << IDM.getInstructionID(orig_next)
					<< "}\n";
			}
		}
	}
}

Instruction *MaxSlicing::find_parent_at_same_level(
		Instruction *x,
		const DenseMap<Instruction *, int> &level,
		const InstMapping &parent) {
#if 0
	Instruction *old_x = clone_map_r.lookup(x);
	assert(old_x);
	errs() << "find_parent_at_same_level: " <<
		old_x->getParent()->getParent()->getName() <<
		"." << old_x->getParent()->getName() << "\n";
	errs() << *old_x << "\n";
#endif
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

void MaxSlicing::assign_level(Instruction *y, Instruction *x,
		DenseMap<Instruction *, int> &level) {
	EdgeType t = get_edge_type(x, y);
	if (t == EDGE_CALL)
		level[y] = level[x] + 1;
	else if (t == EDGE_RET)
		level[y] = level[x] - 1;
	else
		level[y] = level[x];
}

void MaxSlicing::assign_containers(Module &M, Instruction *start) {
	// <start> is a cloned instruction. 
	DenseMap<Instruction *, int> level;
	InstMapping parent;
	level[start] = 0;
	parent[start] = start;

	// Assign each instruction a call-level and a parent. 
	queue<Instruction *> q;
	q.push(start);
	while (!q.empty()) {
		Instruction *x = q.front();
		CFG::iterator it = cfg.find(x);
		if (it != cfg.end()) {
			const InstList &next_insts = it->second;
			for (size_t j = 0, E = next_insts.size(); j < E; ++j) {
				Instruction *y = next_insts[j];
				if (!level.count(y)) {
					assign_level(y, x, level);
					parent[y] = x;
					q.push(y);
				}
			}
		}
		q.pop();
	}

	// DFS from the start. Will traverse all instructions in the CFG. 
	assert(q.empty());
	q.push(start);
	while (!q.empty()) {
		Instruction *x = q.front();
		if (!x->getParent()) {
			assign_container(M, x, level, parent);
			// Continue BFSing.
			const InstList &next_insts = cfg.lookup(x);
			for (size_t j = 0, E = next_insts.size(); j < E; ++j)
				q.push(next_insts[j]);
		}
		q.pop();
	}
}

void MaxSlicing::assign_container(Module &M, Instruction *x,
		const DenseMap<Instruction *, int> &level, const InstMapping &parent) {

	assert(x && "<x> cannot be NULL");
	assert(!x->getParent() && "<x> has a container already");

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
			// TODO: Very fragile. Need a robust way to copy the function
			// prototype. 
			func = Function::Create(
					old_func->getFunctionType(),
					old_func->getLinkage(),
					old_func->getNameStr() + SLICER_SUFFIX,
					&M);
			func->setAttributes(old_func->getAttributes());
			func->setCallingConv(old_func->getCallingConv());
			if (old_func->hasGC())
				func->setGC(old_func->getGC());
			Function::arg_iterator ai = func->arg_begin();
			Function::arg_iterator old_ai = old_func->arg_begin();
			for (; ai != func->arg_end() && old_ai != old_func->arg_end();
					++ai, ++old_ai)
				ai->setName(old_ai->getName());
		} else {
			Instruction *p = find_parent_at_same_level(x, level, parent);
			func = p->getParent()->getParent();
		}
		bb = BasicBlock::Create(getGlobalContext(), old_bb->getName(), func);
	} else {
		Instruction *p = find_parent_at_same_level(x, level, parent);
		bb = p->getParent();
	}
	assert(bb);
	// Append <x> to <bb>.
	bb->getInstList().push_back(x);
	assert(x->getParent());
	assert(x->getParent()->getParent());
}

MaxSlicing::EdgeType MaxSlicing::get_edge_type(Instruction *x, Instruction *y) {

	assert(x && y && "<x> and <y> cannot be NULL");
	x = clone_map_r.lookup(x);
	assert(x && "<x> must be in the cloned CFG");
	y = clone_map_r.lookup(y);
	assert(y && "<y> must be in the cloned CFG");

	Exec &EXE = getAnalysis<Exec>();
	if (is_call(x)) {
		// is_call(x) does not necessarily mean x => y is a call edge. 
		// It may also be the case that the callee is not sliced and <y> is just
		// a successor of the CallInst. 
		Function *fy = y->getParent()->getParent();
		if (y == fy->begin()->begin() && EXE.may_exec_landmark(fy))
			return EDGE_CALL;
	}
	if (is_ret(x))
		return EDGE_RET;
	assert(x->getParent()->getParent() == y->getParent()->getParent());
	return (x->getParent() == y->getParent() ? EDGE_INTRA_BB : EDGE_INTER_BB);
}

Instruction *MaxSlicing::clone_inst(
		int thr_id, size_t trunk_id, const Instruction *x) {
	Instruction *y = x->clone();
	y->setName(x->getName());
	// Remove ins_id metadata. Not an original instruction any more. 
	y->setMetadata("ins_id", NULL);
	/*
	 * Function-local metadata is no longer valid after the instruction 
	 * being cloned to another function. 
	 * Currently, we map them to NULL, but FIXME. 
	 */
	DenseMap<const Value *, Value *> empty_value_map;
	for (unsigned i = 0; i < y->getNumOperands(); ++i) {
		Value *op = y->getOperand(i);
		empty_value_map.clear();
		Value *new_op = MapValue(op, empty_value_map);
		if (new_op)
			y->setOperand(i, new_op);
	}
	// Add the clone_info metadata. 
	unsigned orig_ins_id = getAnalysis<IDManager>().getInstructionID(x);
	assert(orig_ins_id != IDManager::INVALID_ID);
	vector<Value *> ops;
	const IntegerType *int_type = IntegerType::get(x->getContext(), 32);
	ops.push_back(ConstantInt::get(int_type, thr_id));
	ops.push_back(ConstantInt::get(int_type, trunk_id));
	ops.push_back(ConstantInt::get(int_type, orig_ins_id));
	y->setMetadata(
			"clone_info", MDNode::get(x->getContext(), &ops[0], ops.size()));
	return y;
}

void MaxSlicing::build_cfg_of_thread(Module &M, int thr_id) {
	dbgs() << "Building CFG of Thread " << thr_id << "...\n";
	
	clone_map[thr_id].clear();
	assert(trace.count(thr_id));
	const InstList &thr_trace = trace.find(thr_id)->second;
	assert(thr_trace.size() > 0);
	vector<Instruction *> call_stack;
	/*
	 * If there is only one trunk, the control flow contains only one node
	 * and zero edges. 
	 */
	if (thr_trace.size() == 1) {
		Instruction *the_ins = thr_trace[0];
		create_and_link_cloned_inst(thr_id, 0, the_ins);
	} else {
		// Iterate through each trunk except the last one. 
		// The last instruction is automatically added when processing the
		// second-to-last trunk. 
		for (size_t i = 0, E = thr_trace.size(); i + 1 < E; ++i) {
			dbgs() << "Building CFG of Trunk " << i << "...\n";
			DEBUG(dbgs() << "  " << *thr_trace[i] << "\n";
			dbgs() << "  " << *thr_trace[i + 1] << "\n";
			print_call_stack(dbgs(), call_stack););
			// <i> is the trunk ID. 
			build_cfg_of_trunk(thr_trace[i], thr_trace[i + 1],
					thr_id, i, call_stack);
		}
	}

	DEBUG(dump_thr_cfg(cfg, thr_id););
	// Assign containers. 
	assert(!thr_trace.empty());
	assert(clone_map[thr_id].size() > 0);
	Instruction *start = clone_map[thr_id][0].lookup(thr_trace[0]);
	assert(start);
	assign_containers(M, start);
}

void MaxSlicing::print_call_stack(raw_ostream &O, const InstList &cs) {
	O << "=== Call stack ===\n";
	for (size_t i = 0; i < cs.size(); ++i)
		O << "  " << *cs[i] << "\n";
}

void MaxSlicing::build_cfg_of_trunk(Instruction *start, Instruction *end,
		int thr_id, size_t trunk_id, InstList &call_stack) {
	assert(landmarks.count(end));
	
	// DFS in both directions to find the instructions may be
	// visited in the trunk. 
	InstSet visited_nodes; visited_nodes.insert(start);
	EdgeSet visited_edges;
	InstList end_call_stack;
	// Put a tombstone. 
	end_call_stack.push_back(NULL);
	dfs(start, end, visited_nodes, visited_edges, call_stack, end_call_stack);
#if 0
	errs() << "Starting call stack:\n";
	print_call_stack(errs(), call_stack);
#endif
	call_stack = end_call_stack;
	// NOTE: Be careful. <call_stack> already gets changed. 
	if (!visited_nodes.count(end) ||
			(!end_call_stack.empty() && end_call_stack.front() == NULL)) {
		IDManager &IDM = getAnalysis<IDManager>();
		errs() << "=== Cannot reach from <start> to <end> ===\n";
		errs() << IDM.getInstructionID(start) << ":" << *start << "\n";
		errs() << IDM.getInstructionID(end) << ":" << *end << "\n";
		errs() << "Nodes and edges reachable from <start>:\n";
		print_inst_set(errs(), visited_nodes);
		print_edge_set(errs(), visited_edges);
	}
	assert(visited_nodes.count(end) &&
			"Unable to reach from <start> to <end>");
	// <end_call_stack> should be changed. 
	assert(end_call_stack.empty() || end_call_stack.front() != NULL);

	refine_from_end(start, end, visited_nodes, visited_edges);
	// Clone instructions in this trunk. 
	// Note <start> may equal <end>. 
	clone_map[thr_id].push_back(InstMapping());
	forall(InstSet, it, visited_nodes) {
		Instruction *orig = *it;
		// <start> should be already cloned in the last trunk
		// except for the first trunk. 
		// <end> should be cloned into the next trunk. 
		if (orig != end && (orig != start || trunk_id == 0))
			create_and_link_cloned_inst(thr_id, trunk_id, orig);
	}
	// <end> belongs to the next trunk. 
	create_and_link_cloned_inst(thr_id, trunk_id + 1, end);

	DEBUG(print_inst_set(dbgs(), visited_nodes););
	DEBUG(print_edge_set(dbgs(), visited_edges););
	
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

void MaxSlicing::create_and_link_cloned_inst(
		int thr_id, size_t trunk_id, Instruction *orig) {
	// Create
	Instruction *cloned = clone_inst(thr_id, trunk_id, orig);
	// Link
	while (trunk_id >= clone_map[thr_id].size())
		clone_map[thr_id].push_back(InstMapping());
	clone_map[thr_id][trunk_id][orig] = cloned;
	clone_map_r[cloned] = orig;
	cloned_to_trunk[cloned] = trunk_id;
	cloned_to_tid[cloned] = thr_id;
}

void MaxSlicing::refine_from_end(Instruction *start, Instruction *end,
		InstSet &visited_nodes, EdgeSet &visited_edges) {
	// A temporary reverse CFG. 
	CFG cfg_t;
	forall(EdgeSet, it, visited_edges) {
		Instruction *x = it->first, *y = it->second;
		cfg_t[y].push_back(x);
	}
	// DFS from the end.
	InstSet visited_nodes_r;
	EdgeSet visited_edges_r;
	visited_nodes_r.insert(end);
	dfs_cfg(cfg_t, end, visited_nodes_r, visited_edges_r);
	// Refine the visited_nodes and the visited_edges so that
	// only nodes that can be visited in both directions are
	// taken. 
	InstSet orig_visited_nodes(visited_nodes);
	EdgeSet orig_visited_edges(visited_edges);
	visited_nodes.clear();
	visited_edges.clear();
	forall(InstSet, it, orig_visited_nodes) {
		if (visited_nodes_r.count(*it))
			visited_nodes.insert(*it);
	}
	forall(EdgeSet, it, orig_visited_edges) {
		if (visited_edges_r.count(make_pair(it->second, it->first)))
			visited_edges.insert(*it);
	}
}

void MaxSlicing::dfs_cfg(const CFG &cfg, Instruction *x,
		InstSet &visited_nodes, EdgeSet &visited_edges) {
	assert(x && "<x> cannot be NULL");
	assert(visited_nodes.count(x) && "<x>'s parent hasn't been set");
	const InstList &next_insts = cfg.lookup(x);
	for (size_t j = 0, E = next_insts.size(); j < E; ++j) {
		Instruction *y = next_insts[j];
		visited_edges.insert(make_pair(x, y));
		if (!visited_nodes.count(y)) {
			// Has not visited <y>. 
			visited_nodes.insert(y);
			if (!landmarks.count(y))
				dfs_cfg(cfg, y, visited_nodes, visited_edges);
		}
	}
}

void MaxSlicing::dfs(Instruction *x, Instruction *end,
		InstSet &visited_nodes, EdgeSet &visited_edges,
		InstList &call_stack, InstList &end_call_stack) {
	assert(x && "<x> cannot be NULL");
	assert(visited_nodes.count(x));

	DEBUG(dbgs() << "dfs:" << *x << "\n";);
	DEBUG(print_call_stack(dbgs(), call_stack););

	// We are performing intra-thread analysis now. 
	// Don't go to the thread function. 
	if (is_call(x) && !is_pthread_create(x)) {
		bool may_exec_landmark = false;
		CallGraphFP &CG = getAnalysis<CallGraphFP>();
		Exec &EXE = getAnalysis<Exec>();
		const FuncList &callees = CG.get_called_functions(x);
		for (size_t j = 0, E = callees.size(); j < E; ++j) {
			if (EXE.may_exec_landmark(callees[j]))
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
				move_on(x, y, end,
						visited_nodes, visited_edges,
						call_stack, end_call_stack);
				assert(call_stack.back() == x);
				call_stack.pop_back();
			}
			return;
		}
		// If the callee cannot execute any landmark,
		// treat it as a regular instruction without
		// going into the function body. 
	} // if is_call

	if (is_ret(x)) {
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
		Instruction *bkp = call_stack.back();
		call_stack.pop_back();
		move_on(x, y, end,
				visited_nodes, visited_edges,
				call_stack, end_call_stack);
		call_stack.push_back(bkp);
		return;
	} // if is_ret

	if (!x->isTerminator()) {
		BasicBlock::iterator y = x; ++y;
		move_on(x, y, end,
				visited_nodes, visited_edges,
				call_stack, end_call_stack);
	} else {
		TerminatorInst *ti = dyn_cast<TerminatorInst>(x);
		for (unsigned j = 0, E = ti->getNumSuccessors(); j < E; ++j) {
			Instruction *y = ti->getSuccessor(j)->begin();
			move_on(x, y, end,
					visited_nodes, visited_edges,
					call_stack, end_call_stack);
		}
	}
}

void MaxSlicing::move_on(Instruction *x, Instruction *y, Instruction *end,
		InstSet &visited_nodes, EdgeSet &visited_edges,
		InstList &call_stack, InstList &end_call_stack) {
	DEBUG(dbgs() << "move_on:" << *y << "\n";);
	/*
	 * No matter whether <y> is in the cut, we mark <y> and <x, y>
	 * as visited. But we don't continue traversing <y> if <y>
	 * is in the cut.
	 */
	visited_edges.insert(make_pair(x, y));
	// Don't put it in the "if (!visited_nodes.count(y))". 
	// <y> might be visited as the starting point. 
	if (y == end) {
		end_call_stack.clear();
		end_call_stack.insert(end_call_stack.end(),
				call_stack.begin(), call_stack.end());
	}
	if (!visited_nodes.count(y)) {
		visited_nodes.insert(y);
		if (!landmarks.count(y))
			dfs(y, end, visited_nodes, visited_edges, call_stack, end_call_stack);
	}
}

bool MaxSlicing::is_sliced(const Function *f) {
	// The main function is a little special. 
	// Function main is the cloned function, and function main.OLDMAIN is
	// the original function. 
	if (is_main(f))
		return true;
	return f->getNameStr().find(SLICER_SUFFIX) != string::npos;
}

void MaxSlicing::find_invoke_successors(Module &M) {
	invoke_successors.clear();
	forallconst(Trace, it, trace) {
		assert(!it->second.empty());
		Instruction *old_start = it->second[0];
		if (clone_map[it->first].empty())
			continue;
		Instruction *new_start = clone_map[it->first][0].lookup(old_start);
		assert(new_start);
		find_invoke_successors_from(M, new_start);
	}
}

void MaxSlicing::find_invoke_successors_from(Module &M, Instruction *start) {
	InstSet visited;
	// TODO: Call stacks are too expensive in a BFS queue. 
	queue<pair<Instruction *, InstList> > q;
	q.push(make_pair(start, InstList()));
	visited.insert(start);
	while (!q.empty()) {
		Instruction *x = q.front().first;
		const InstList &call_stack = q.front().second;
		const InstList &next_insts = cfg.lookup(x);
		for (size_t j = 0, E = next_insts.size(); j < E; ++j) {
			Instruction *y = next_insts[j];
			if (visited.count(y))
				continue;
			visited.insert(y);
			if (get_edge_type(x, y) == EDGE_CALL) {
				q.push(make_pair(y, call_stack));
				q.back().second.push_back(x);
			} else if (get_edge_type(x, y) == EDGE_RET) {
				assert(call_stack.size() > 0 && "The call stack is empty");
				Instruction *ret_addr = call_stack.back();
				if (isa<InvokeInst>(ret_addr)) {
					DEBUG(dbgs() << "Invoke successor:\n";);
					DEBUG(dbgs() << *ret_addr << "\n" << *y << "\n";);
					invoke_successors[ret_addr].push_back(y);
				}
				q.push(make_pair(y, call_stack));
				q.back().second.pop_back();
			} else {
				q.push(make_pair(y, call_stack));
			}
		}
		q.pop();
	}
}
