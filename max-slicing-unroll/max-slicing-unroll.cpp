/**
 * Author: Jingyue
 *
 * FIXME: Does not handle recursive functions for now.
 * FIXME: Remove unnecessary parameters M. 
 */

#include "llvm/Module.h"
#include "llvm/LLVMContext.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Instructions.h"
#include "llvm/Constants.h"
#include "llvm/BasicBlock.h"
#include "llvm/Pass.h"
#include "llvm/Support/CFG.h"
#include "llvm/Transforms/Utils/BasicInliner.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"
#include "llvm/Support/CommandLine.h"

#include "config.h"
#include "max-slicing-unroll.h"
#include "idm/id.h"
#include "common/callgraph-fp/callgraph-fp.h"
#include "common/reach/reach.h"
#include "common/may-exec/may-exec.h"

#include <iostream>
#include <fstream>
#include <sstream>

using namespace llvm;
using namespace std;

namespace {
	static RegisterPass<slicer::MaxSlicingUnroll>
		X("max-slicing-unroll",
				"Unroll the program according to the trace",
				false,
				true);
	static cl::opt<string> TraceFile(
			"trace",
			cl::NotHidden,
			cl::desc("A trace of instructions according to which we will slice"
				" the program"),
			cl::init(""));
	static cl::opt<string> CutFile(
			"cut",
			cl::NotHidden,
			cl::desc("Set of instructions that should be traced"),
			cl::init(""));
}

namespace slicer {

	void MaxSlicingUnroll::getAnalysisUsage(AnalysisUsage &AU) const {
		AU.addRequired<ObjectID>();
		AU.addRequired<CallGraphFP>();
		AU.addRequired<MayExec>();
		ModulePass::getAnalysisUsage(AU);
	}

	MaxSlicingUnroll::MaxSlicingUnroll(): ModulePass(&ID) {}

	int MaxSlicingUnroll::read_trace(
			const string &trace_file,
			Trace &trace,
			vector<ThreadCreationRecord> &thr_cr_records) {
		if (trace_file == "") {
			cerr << "Didn't specify the trace file. Do nothing.\n";
			return -1;
		}
		ifstream fin(trace_file.c_str());
		if (!fin) {
			cerr << "Cannot open file " << trace_file << endl;
			return -1;
		}

		trace.clear();
		thr_cr_records.clear();
		string line;
		ObjectID &IDM = getAnalysis<ObjectID>();

		while (getline(fin, line)) {
			istringstream iss(line);
			int thr_id, ins_id, child_tid;
			if (iss >> thr_id >> ins_id >> child_tid) {
				Instruction *ins = IDM.getInstruction(ins_id);
				if (ins == NULL) {
					cerr << "Cannot find the instruction with ID " << ins_id << endl;
					return -1;
				}
				trace[thr_id].push_back(ins);
				if (child_tid != -1) {
					// A thread creation
					thr_cr_records.push_back(
							ThreadCreationRecord(
								thr_id,
								trace[thr_id].size() - 1,
								child_tid));
				}
			}
		}
		
		forall(Trace, it, trace) {
			if (it->second.size() < 2) {
				cerr << "There should be at least two entries in a trace\n";
				return -1;
			}
		}
		
		return 0;
	}

	int MaxSlicingUnroll::read_cut(
			const string &cut_file,
			InstSet &cut) {
		if (cut_file == "") {
			cerr << "Didn't specify the cut file. Do nothing.\n";
			return -1;
		}
		ifstream fin(cut_file.c_str());
		string line;
		ObjectID &IDM = getAnalysis<ObjectID>();
		cut.clear();
		while (getline(fin, line)) {
			istringstream iss(line);
			int i;
			if (iss >> i) {
				Instruction *ins = IDM.getInstruction(i);
				if (ins == NULL) {
					cerr << "Cannot find the instruction with ID " << i << endl;
					return -1;
				}
				cut.insert(ins);
			}
		}
		return 0;
	}

	int MaxSlicingUnroll::read_trace_and_cut(
			const string &trace_file,
			const string &cut_file,
			Trace &trace,
			vector<ThreadCreationRecord> &thr_cr_records,
			InstSet &cut) {
		// Read the trace. 
		if (read_trace(trace_file, trace, thr_cr_records))
			return -1;
		// Collect all instructions that should be recorded once executed. 
		if (read_cut(cut_file, cut))
			return -1;
		// <cut> should also contain all instructions in the trace. 
		for (Trace::iterator it = trace.begin();
				it != trace.end(); ++it) {
			for (size_t i = 0, E = it->second.size(); i < E; ++i)
				cut.insert(it->second[i]);
		}
		assert(!trace.empty() && "A trace cannot be empty");
		return 0;
	}

	void MaxSlicingUnroll::print_inst_set(const InstSet &s) {
		ObjectID &IDM = getAnalysis<ObjectID>();
		vector<unsigned> inst_ids;
		for (InstSet::const_iterator it = s.begin();
				it != s.end(); ++it)
			inst_ids.push_back(IDM.getInstructionID(*it));
		sort(inst_ids.begin(), inst_ids.end());
		forall(vector<unsigned>, it, inst_ids)
			cerr << *it << " ";
		cerr << endl;
	}

	void MaxSlicingUnroll::print_edge_set(const EdgeSet &s) {
		ObjectID &IDM = getAnalysis<ObjectID>();
		vector<pair<unsigned, unsigned> > edge_ids;
		forallconst(EdgeSet, it, s) {
			edge_ids.push_back(make_pair(
						IDM.getInstructionID(it->first),
						IDM.getInstructionID(it->second)));
		}
		sort(edge_ids.begin(), edge_ids.end());
		for (size_t i = 0; i < edge_ids.size(); ++i)
			cerr << "(" << edge_ids[i].first << "," << edge_ids[i].second << ") ";
		cerr << endl;
	}

	void MaxSlicingUnroll::add_cfg_edge(
			Instruction *x,
			Instruction *y) {
		assert(clone_map_r.count(x) && "<x> must be in the cloned CFG");
		assert(clone_map_r.count(y) && "<y> must be in the cloned CFG");
		cfg[x].push_back(y);
		cfg_r[y].push_back(x);
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
		Instruction *start = clone_map[thr_id][0].lookup(thr_trace[0]);
		assert(start);
		assign_containers(M, start);
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
		assert(visited_nodes.count(end) &&
				"Unable to reach from <start> to <end>");
		refine(start, end, cut, visited_nodes, visited_edges);
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

	void MaxSlicingUnroll::refine(
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
	
	void MaxSlicingUnroll::dfs(
			Instruction *x,
			const InstSet &cut,
			InstSet &visited_nodes,
			EdgeSet &visited_edges,
			InstList &call_stack) {

		assert(x && "<x> cannot be NULL");
		assert(visited_nodes.count(x));

		if (is_call(x) && !is_intrinsic_call(x)) {
			bool may_exec_landmark = false;
			CallGraphFP &CG = getAnalysis<CallGraphFP>();
			MayExec &ME = getAnalysis<MayExec>();
			const FuncList &callees = CG.get_called_functions(x);
			for (size_t j = 0, E = callees.size(); j < E; ++j) {
				if (ME.may_exec_landmark(callees[j]))
					may_exec_landmark = true;
			}
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

	void MaxSlicingUnroll::print_levels_in_thread(
			int thr_id,
			const DenseMap<Instruction *, int> &level) {
		vector<pair<pair<size_t, unsigned>, int> > leveled;
		for (size_t i = 0; i < clone_map[thr_id].size(); ++i) {
			forall(InstMapping, it, clone_map[thr_id][i]) {
				ObjectID &IDM = getAnalysis<ObjectID>();
				if (level.count(it->second)) {
					leveled.push_back(make_pair(
								make_pair(i, IDM.getInstructionID(it->first)),
								level.lookup(it->second)));
				}
			}
		}
		sort(leveled.begin(), leveled.end());
		for (size_t i = 0; i < leveled.size(); ++i) {
			cerr << "{" << leveled[i].first.first << ":"
				<< leveled[i].first.second << "} = " << leveled[i].second << endl;
		}
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

	void MaxSlicingUnroll::redirect_program_entry(
			Module &M,
			Instruction *old_start) {
		assert(!clone_map.empty());
		assert(clone_map.count(0));
		// The main thread ID is always 0. 
		Instruction *start = clone_map[0][0].lookup(old_start);
		assert(start && "Cannot find the program entry in the cloned CFG");
		Function *main = start->getParent()->getParent();
		Function *old_main = old_start->getParent()->getParent();
		old_main->deleteBody();
		BasicBlock *bb = BasicBlock::Create(
				getGlobalContext(),
				"the_only",
				old_main);
		vector<Value *> args;
		for (Function::arg_iterator ai = old_main->arg_begin();
				ai != old_main->arg_end(); ++ai)
			args.push_back(ai);
		Value *ret = CallInst::Create(main, args.begin(), args.end(), "", bb);
		ReturnInst::Create(getGlobalContext(), ret, bb);
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

	bool MaxSlicingUnroll::runOnModule(Module &M) {
		// Read the trace and the cut. 
		Trace trace;
		vector<ThreadCreationRecord> thr_cr_records;
		InstSet cut;
		if (read_trace_and_cut(TraceFile, CutFile, trace, thr_cr_records, cut))
			return false;

		// Which functions may execute a landmark? 
		MayExec &ME = getAnalysis<MayExec>();
		ME.setup_landmarks(cut);
		ME.run();

		// Build the control flow graph. 
		// Output to <cfg>. 
		build_cfg(M, trace, cut);
		// Fix the def-use graph. 
		fix_def_use(M, trace);

		// Stat before the clone mappings become invalid.
		stat(M);

#if 0
		// Redirect the program entry to main.TERN. 
		// Remove all instructions in main, and add a call to main.TERN instead. 
		// Note that this will invalidate some entries in <clone_map> and
		// <clone_map_r>. 
		assert(trace.count(0));
		redirect_program_entry(M, trace[0][0]);
#endif
		// Link thread functions. 
		link_thr_funcs(M, trace, thr_cr_records);

#ifdef VERBOSE
		cerr << "Dumping module...\n";
		M.dump();
#endif

		return true;
	}

	void MaxSlicingUnroll::stat(Module &M) {
		cerr << "Stat...\n";
		unsigned n_orig_insts = 0;
		InstSet cloned_orig_insts;
		forallinst(M, ii) {
			Instruction *orig = clone_map_r.lookup(ii);
			if (!orig)
				n_orig_insts++;
			else
				cloned_orig_insts.insert(orig);
		}
		cerr << cloned_orig_insts.size() << " out of " << n_orig_insts
			<< " instructions are still in the sliced program.\n";
	}

	void MaxSlicingUnroll::fix_def_use_bb(
			Module &M) {
		cerr << "Fixing BBs in def-use graph...\n";
		DenseMap<Function *, BasicBlock *> unreach_bbs;
		for (Module::iterator fi = M.begin(); fi != M.end(); ++fi) {
			for (Function::iterator bi = fi->begin(); bi != fi->end(); ++bi) {
				assert(bi->begin() != bi->end());
				// Skip original BBs. 
				if (!clone_map_r.count(bi->begin()))
					continue;
				if (isa<PHINode>(bi->begin())) {
					// Fix all PHINodes in this BB. 
					BBMapping actual_pred_bbs;
					const InstList &actual_pred_insts = cfg_r.lookup(bi->begin());
					for (size_t j = 0; j < actual_pred_insts.size(); ++j) {
						Instruction *orig_pred_inst =
							clone_map_r.lookup(actual_pred_insts[j]);
						assert(orig_pred_inst);
						actual_pred_bbs[orig_pred_inst->getParent()] =
							actual_pred_insts[j]->getParent();
					}
					for (BasicBlock::iterator ii = bi->begin();
							ii != (BasicBlock::iterator)bi->getFirstNonPHI(); ++ii) {
						PHINode *phi = dyn_cast<PHINode>(ii);
						for (unsigned j = phi->getNumIncomingValues(); j > 0;) {
							--j;
							BasicBlock *incoming_bb = phi->getIncomingBlock(j);
							// If an incoming block does not appear in the cloned CFG, 
							// we remove this incoming value;
							// otherwise, we replace it with its counterpart in the
							// cloned CFG. 
							if (!actual_pred_bbs.count(incoming_bb))
								// Don't remove PHINodes even if it's empty. 
								// Dangerous because the clone mappings still have its
								// references. 
								phi->removeIncomingValue(j, false);
							else
								phi->setIncomingBlock(j, actual_pred_bbs[incoming_bb]);
						}
					}
				}
				// Fix the terminator. 
				TerminatorInst *ti = bi->getTerminator();
				if (ti == NULL) {
					// Some BBs may end with an exit landmark, e.g. 
					// pthread_exit() and exit(), and thus they don't have a Terminator.
					// Need to append UnreachableInsts to them
					// because LLVM requires all BBs to end with a TerminatorInst. 
					new UnreachableInst(getGlobalContext(), bi);
				} else {
					BBMapping actual_succ_bbs; // Map to the cloned CFG. 
					const InstList &actual_succ_insts = cfg.lookup(ti);
					for (size_t j = 0; j < actual_succ_insts.size(); ++j) {
						Instruction *orig_succ_inst =
							clone_map_r.lookup(actual_succ_insts[j]);
						assert(orig_succ_inst);
						actual_succ_bbs[orig_succ_inst->getParent()] =
							actual_succ_insts[j]->getParent();
					}
					for (unsigned j = 0; j < ti->getNumSuccessors(); ++j) {
						// If an outcoming block does not appear in the cloned CFG, 
						// we redirect the outcoming edge to the unreachable BB in
						// the function;
						// otherwise, redirect the outcoming edge to the cloned CFG. 
						BasicBlock *outcoming_bb = ti->getSuccessor(j);;
						if (!actual_succ_bbs.count(outcoming_bb)) {
							if (!unreach_bbs.count(fi)) {
								// <fi> does not have an unreachable BB yet. Create one. 
								// This statement changes <fi> while we are still scanning
								// <fi>, but it's fine because the BB list is simply a
								// linked list.
								BasicBlock *unreachable_bb = BasicBlock::Create(
										getGlobalContext(),
										"unreachable",
										fi);
								Constant *func_abort = M.getOrInsertFunction(
										"abort",
										FunctionType::get(
											Type::getVoidTy(getGlobalContext()),
											false));
								assert(func_abort && "Cannot find function abort()");
								CallInst::Create(func_abort, "", unreachable_bb);
								new UnreachableInst(getGlobalContext(), unreachable_bb);
								unreach_bbs[fi] = unreachable_bb;
							}
							ti->setSuccessor(j, unreach_bbs[fi]);
						} else {
							ti->setSuccessor(j, actual_succ_bbs[outcoming_bb]);
						}
					}
				}
			}
		}
#ifdef CHECK
		forallbb(M, bi) {
			unsigned n_terminators = 0;
			forall(BasicBlock, ii, *bi) {
				if (ii->isTerminator())
					n_terminators++;
			}
			assert(n_terminators == 1);
		}
#endif
	}

	Instruction *MaxSlicingUnroll::find_op_in_cloned(
			Instruction *op,
			Instruction *user,
			const InstMapping &parent) {
		// <op'> must dominate <user>, therefore we trace back via <parent>.
		Instruction *op2 = user;
		do {
			Instruction *old_op2 = op2;
			op2 = parent.lookup(op2);
			assert(op2);
			assert(op2 != old_op2 && "Cannot find the corresponding operand");
			assert(clone_map_r.count(op2));
		} while (clone_map_r.lookup(op2) != op);
		return op2;
	}

	void MaxSlicingUnroll::fix_def_use(
			Module &M,
			const Trace &trace) {
		/*
		 * Things to fix:
		 * . BBs used in PHINodes and TerminatorInsts. 
		 * . Instructions. 
		 * . Function parameters
		 * . Functions used in call instructions. 
		 */
		// TODO: Each fix_def_use_ function iterates through all cloned
		// instructions for now. Could make it faster by maintaining a list
		// of unresolved operands. 
		cerr << "\nFixing def-use...\n";
		fix_def_use_bb(M);
		fix_def_use_insts(M, trace);
		fix_def_use_func_param(M);
		fix_def_use_func_call(M);
		cerr << "Done fix_def_use\n";
	}

	void MaxSlicingUnroll::print_cloned_inst(Instruction *ins) {
		assert(clone_map_r.count(ins));
		assert(cloned_to_trunk.count(ins));
		Instruction *ii = clone_map_r.lookup(ins);
		cerr << "===== print_cloned_inst =====\n";
		cerr << ii->getParent()->getParent()->getNameStr() << "."
			<< ii->getParent()->getNameStr() << endl;
		ii->dump();
		ObjectID &IDM = getAnalysis<ObjectID>();
		cerr << "trunk = " << cloned_to_trunk.lookup(ins) << "; "
			<< "ID = " << IDM.getInstructionID(ii) << endl;
	}

	void MaxSlicingUnroll::fix_def_use_func_call(Module &M) {
		cerr << "Fixing function calls in def-use graph...\n";
		forall(InstMapping, it, clone_map_r) {
			Instruction *ins = it->first;
			if (is_call(ins) && !is_intrinsic_call(ins)) {
				const InstList &next_insts = cfg.lookup(ins);
				Function *callee = NULL;
				for (size_t j = 0; j < next_insts.size(); ++j) {
					if (get_edge_type(ins, next_insts[j]) == EDGE_CALL) {
						// FIXME: does not handle function pointers. 
						assert(callee == NULL && "For now, we allow only one target");
						callee = next_insts[j]->getParent()->getParent();
					}
				}
				// Sometimes, the call edges get removed by the refinement process. 
				// In that case, we simply let the instruction call the original
				// function; otherwise, we replace the target with <callee>. 
				if (callee) {
					CallSite cs(ins);
					// I guess this function works for both function pointers and
					// function definitions. 
					assert(callee->getType() == cs.getCalledValue()->getType());
					cs.setCalledFunction(callee);
				}
			}
		}
	}

	void MaxSlicingUnroll::fix_def_use_func_param(Module &M) {
		cerr << "Fixing function parameters in def-use graph...\n";
		forall(InstMapping, it, clone_map_r) {
			Instruction *ins = it->first;
			Instruction *orig_ins = it->second;
			Function *func = ins->getParent()->getParent();
			Function *orig_func = orig_ins->getParent()->getParent();
			ValueMapping params_map;
			Function::arg_iterator ai = func->arg_begin();
			Function::arg_iterator orig_ai = orig_func->arg_begin();
			for (; ai != func->arg_end() && orig_ai != orig_func->arg_end();
					++ai, ++orig_ai)
				params_map[orig_ai] = ai;
			assert(ai == func->arg_end() && orig_ai == orig_func->arg_end() &&
					"<func> and <orig_func> should have the same number of arguments");
			for (unsigned j = 0; j < ins->getNumOperands(); ++j) {
				Value *arg = params_map.lookup(ins->getOperand(j));
				if (arg)
					ins->setOperand(j, arg);
			}
		}
	}

	void MaxSlicingUnroll::fix_def_use_insts(
			Module &M,
			const Trace &trace) {
		cerr << "Fixing instructions in def-use graph...\n";
		// Construct the DFS tree. 
		InstMapping parent;
		// We borrow assign_level to calculate <parent>. 
		// <level> is actually unused.
		DenseMap<Instruction *, int> level;
		forallconst(Trace, it, trace) {
			Instruction *start = clone_map[it->first][0].lookup(it->second[0]);
			assert(start);
			parent[start] = start;
			assign_level(start, level, parent);
		}
		// In SSA, a definition dominates all its uses. Therefore, if we trace
		// back from a use via the DFS tree, we should always be able to find
		// its definition. 
		forall(InstMapping, it, clone_map_r) {
			Instruction *user = it->first;
			assert(user);
			if (!isa<PHINode>(user)) {
				for (unsigned j = 0; j < user->getNumOperands(); ++j) {
					Instruction *op = dyn_cast<Instruction>(user->getOperand(j));
					// Only fix instructions at this stage. 
					if (op == NULL)
						continue;
					// <op> is still in the original CFG.
					// Need to find its latest countepart <op'>. 
					user->setOperand(j, find_op_in_cloned(op, user, parent));
				}
			} else {
				// PHINode. Similar to non-PHINode case. 
				// Start tracing back from the tail of each incoming edge. 
				PHINode *phi = dyn_cast<PHINode>(user);
				for (unsigned j = 0; j < phi->getNumIncomingValues(); ++j) {
					Instruction *op = dyn_cast<Instruction>(phi->getIncomingValue(j));
					if (op == NULL)
						continue;
					Instruction *op2 = find_op_in_cloned(
								op,
								phi->getIncomingBlock(j)->getTerminator(),
								parent);
					phi->setIncomingValue(j, op2);
				}
			}
		}
	}

	void MaxSlicingUnroll::link_thr_func(
			Module &M,
			const Trace &trace,
			int parent_tid,
			size_t trunk_id,
			int child_tid) {
		// pthread_create in the original program. 
		Trace::const_iterator it = trace.find(parent_tid);
		assert(it != trace.end());
		Instruction *orig_site = it->second[trunk_id];
		// pthread_create in the cloned program. 
		assert(clone_map.count(parent_tid));
		Instruction *new_site = clone_map[parent_tid][trunk_id].lookup(orig_site);
		assert(new_site &&
				"Cannot find the thread creation site in the cloned CFG");
		// Old thread entry.
		Trace::const_iterator j = trace.find(child_tid);
		assert(j != trace.end());
		Instruction *orig_entry = j->second[0];
		// New thread entry.
		assert(clone_map.count(child_tid));
		Instruction *new_entry = clone_map[child_tid][0].lookup(orig_entry);
		assert(new_entry &&
				"Cannot find the thread entry in the cloned program.");
		// The thread function in the cloned program. 
		Function *thr_func = new_entry->getParent()->getParent();
		// Replace the target function in pthread_create to <thr_func>.
		assert(is_call(new_site) && !is_intrinsic_call(new_site));
		CallSite cs(new_site);
		Function *callee = cs.getCalledFunction();
		assert(callee && (callee->getNameStr() == "pthread_create" ||
					callee->getNameStr() == "tern_wrap_pthread_create"));
		unsigned arg_no;
		if (callee->getNameStr() == "pthread_create") {
			// pthread_create(&t, NULL, foo, ...)	
			assert(cs.arg_size() >= 3 &&
					"A pthread_create must have at least 3 arguments.");
			arg_no = 2;
		} else {
			// tern_wrap_pthread_create(unknown, unknown, &t, NULL, foo, ...)
			assert(cs.arg_size() >= 5 &&
					"A tern_wrap_pthread_create must have at least 5 arguments.");
			arg_no = 4;
		}
		if (cs.getArgument(arg_no)->getType() == thr_func->getType())
			cs.setArgument(arg_no, thr_func);
		else {
			// <thr_func> may not have the same signature as required by the
			// pthread_create, because our alias analysis catches bitcast.
			Value *wrapped_thr_func = new BitCastInst(
					thr_func,
					cs.getArgument(arg_no)->getType(),
					"",
					new_site);
			cs.setArgument(arg_no, wrapped_thr_func);
		}
	}

	void MaxSlicingUnroll::link_thr_funcs(
			Module &M,
			const Trace &trace,
			const vector<ThreadCreationRecord> &thr_cr_records) {
		for (size_t i = 0, E = thr_cr_records.size(); i < E; ++i) {
			link_thr_func(
					M,
					trace,
					thr_cr_records[i].parent_tid,
					thr_cr_records[i].trunk_id,
					thr_cr_records[i].child_tid);
		}
	}

	void MaxSlicingUnroll::print(raw_ostream &O, const Module *M) const {
	}

	char MaxSlicingUnroll::ID = 0;
}

