#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_os_ostream.h"

#include "config.h"
#include "racy-pairs.h"
#include "idm/id.h"
#include "bc2bdd/BddAliasAnalysis.h"
#include "common/include/util.h"
#include "llvm-instrument/trace-manager/trace-manager.h"

#include <fstream>
#include <sstream>

using namespace llvm;
using namespace repair;
using namespace std;
using namespace tern;

namespace {
	static RegisterPass<slicer::RacyPairs> X(
			"racy-pairs",
			"Dumps all racy pairs",
			false,
			true); // is analysis
	static cl::opt<string> CloneMapFile(
			"clone-map",
			cl::NotHidden,
			cl::ValueRequired,
			cl::desc("The clone mapping"));
	// We need this to get the trunk ID and therefore use the clone mapping.
	static cl::opt<string> TraceFile(
			"trace",
			cl::NotHidden,
			cl::ValueRequired,
			cl::desc("The landmark trace"));
	static cl::opt<bool> Cloned(
			"cloned",
			cl::NotHidden,
			cl::desc("True if working on the cloned program; False if working "
				"on the original program"),
			cl::init(false));
	static cl::opt<int> SampleRate(
			"sample",
			cl::NotHidden,
			cl::desc("sample rate = 1 / sample"),
			cl::init(1));
}

namespace slicer {

	void RacyPairs::print_inst(Instruction *ins, raw_ostream &O) const {
		BasicBlock *bb = ins->getParent();
		Function *func = bb->getParent();
		O << func->getNameStr() << ":" << bb->getNameStr() << ":\t";
		ins->print(O);
		O << "\n";
	}

	void RacyPairs::dump_inst(Instruction *ins) const {
		BasicBlock *bb = ins->getParent();
		Function *func = bb->getParent();
		cerr << func->getNameStr() << ":" << bb->getNameStr() << ":\t";
		ins->dump();
	}

	void RacyPairs::print(raw_ostream &O, const Module *M) const {
		cerr << "# of racy pairs = " << racy_pairs.size() << endl;
		forallconst(vector<InstPair>, it, racy_pairs) {
			if ((it - racy_pairs.begin()) % 10000 == 0)
				cerr << "Progress: " << it - racy_pairs.begin()
					<< "/" << racy_pairs.size() << endl;
			O << string(10, '=') << "\n";
			print_inst(it->first, O);
			print_inst(it->second, O);
		}
	}

	void RacyPairs::read_trunks(const string &trace_file) {

		ifstream fin(trace_file.c_str());
		assert(fin && "Cannot open the specified landmark trace");

		trunks.clear();
		string line;
		while (getline(fin, line)) {
			istringstream iss(line);
			int idx;
			if (iss >> idx) {
				TraceManager &TM = getAnalysis<TraceManager>();
				trunks[TM.get_record(idx).thr_id].push_back(idx);
			}
		}
	}

	void RacyPairs::read_clone_map(const string &clone_map_file) {
		assert(clone_map_file.length() > 0 && "Didn't specify -clone-map");
		ifstream fin(clone_map_file.c_str());
		assert(fin && "Cannot open the specified clone map file");

		clone_map.clear();
		string line;
		while (getline(fin, line)) {
			istringstream iss(line);
			int thr_id;
			size_t trunk_id;
			unsigned orig_id, cloned_id;
			if (iss >> thr_id >> trunk_id >> orig_id >> cloned_id) {
				while (trunk_id >= clone_map[thr_id].size())
					clone_map[thr_id].push_back(InstMapping());
				ObjectID &IDM = getAnalysis<ObjectID>();
				Instruction *orig = IDM.getInstruction(orig_id);
				Instruction *cloned = IDM.getInstruction(cloned_id);
				assert(orig && cloned);
				assert(orig->getOpcode() == cloned->getOpcode());
				clone_map[thr_id][trunk_id][orig] = cloned;
			}
		}
	}

	void RacyPairs::select_load_store(
			unsigned s, unsigned e,
			int tid,
			bool cloned,
			vector<pair<Instruction *, CallStack> > &load_stores) {
		load_stores.clear();
		for (unsigned p = s; p < e; ++p) {
			if (getAnalysis<TraceManager>().get_record(p).thr_id != tid)
				continue;
			Instruction *ins = compute_inst(p, tid, cloned);
			if (!isa<LoadInst>(ins) && !isa<StoreInst>(ins))
				continue;
#ifdef CONTEXT
			AddCallingContext &ACC = getAnalysis<AddCallingContext>();
			load_stores.push_back(make_pair(ins, ACC.get_calling_context(p)));
#else
			load_stores.push_back(make_pair(ins, CallStack()));
#endif
		}
#ifdef VERBOSE
		cerr << "# of loads/stores = " << load_stores.size() << endl;
#endif
	}

	void RacyPairs::extract_racy_pairs(
			int t1, unsigned s1, unsigned e1,
			int t2, unsigned s2, unsigned e2) {

		BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
		
		// Select only load/store instructions. 
		vector<pair<Instruction *, CallStack> > b1, b2;
		select_load_store(s1, e1, t1, Cloned, b1);
		select_load_store(s2, e2, t2, Cloned, b2);
		// TODO: Can be optimized. a1 and a2 are ordered. 
		for (size_t i1 = 0; i1 < b1.size(); ++i1) {
			Instruction *ins1 = b1[i1].first;
			assert(isa<LoadInst>(ins1) || isa<StoreInst>(ins1));
			for (size_t i2 = 0; i2 < b2.size(); ++i2) {
				Instruction *ins2 = b2[i2].first;
				assert(isa<LoadInst>(ins2) || isa<StoreInst>(ins2));
				// If both are reads, not a race. 
				if (isa<LoadInst>(ins1) && isa<LoadInst>(ins2))
					continue;
				// Perform sampling
				++counter;
				if (counter < SampleRate)
					continue;
				counter = 0;
				// Actual query to bc2bdd
				Value *v1 = (isa<LoadInst>(ins1) ?
						dyn_cast<LoadInst>(ins1)->getPointerOperand() :
						dyn_cast<StoreInst>(ins1)->getPointerOperand());
				Value *v2 = (isa<LoadInst>(ins2) ?
						dyn_cast<LoadInst>(ins2)->getPointerOperand() :
						dyn_cast<StoreInst>(ins2)->getPointerOperand());
#ifdef CONTEXT
				vector<User *> ctxt1, ctxt2;
				ctxt1 = compute_context(b1[i1].second, t1, Cloned);
				ctxt2 = compute_context(b2[i2].second, t2, Cloned);
				print_context(b1[i1].second);
				dump_inst(ins1);
				print_context(b2[i2].second);
				dump_inst(ins2);
				if (BAA.alias(&ctxt1, v1, 0, &ctxt2, v2, 0)) {
					racy_pairs.push_back(make_pair(ins1, ins2));
				}
#else
				if (BAA.alias(v1, 0, v2, 0))
					racy_pairs.push_back(make_pair(ins1, ins2));
#endif
			}
		}
	}

	void RacyPairs::print_context(const CallStack &cs) const {
		cerr << "Context:";
		for (size_t i = 0, E = cs.size(); i < E; ++i)
			cerr << ' ' << cs[i];
		cerr << endl;
	}

	vector<User *> RacyPairs::compute_context(
			const CallStack &cs,
			int thr_id,
			bool cloned) const {
		vector<User *> res;
		for (size_t i = 0, E = cs.size(); i < E; ++i)
			res.push_back(compute_inst(cs[i], thr_id, cloned));
		return res;
	}

	Instruction *RacyPairs::compute_inst(
			unsigned idx,
			int tid,
			bool cloned) const {
		assert(trunks.count(tid) && clone_map.count(tid));
		const vector<unsigned> &thr_trunks = trunks.find(tid)->second;
		const vector<InstMapping> &thr_clone_map = clone_map.find(tid)->second;
		TraceManager &TM = getAnalysis<TraceManager>();
		Instruction *ins = TM.get_record(idx).ins;
		if (cloned) {
			size_t tr = (upper_bound(
						thr_trunks.begin(),
						thr_trunks.end(),
						idx) - thr_trunks.begin()) - 1;
			assert(tr < thr_clone_map.size());
			Instruction *c = thr_clone_map[tr].lookup(ins);
			if (c)
				ins = c;
		}
		return ins;
	}

	size_t RacyPairs::find_next_enforce(
			const vector<unsigned> &indices,
			size_t j) const {
		size_t k = j + 1;
		while (k < indices.size()) {
			TraceManager &TM = getAnalysis<TraceManager>();
			TraceRecord record = TM.get_record(indices[k]);
			if (record.type == TR_LANDMARK_ENFORCE)
				break;
			++k;
		}
		return k;
	}

	bool RacyPairs::runOnModule(Module &M) {
#if 1
		ObjectID &IDM = getAnalysis<ObjectID>();
		vector<User *> ctxt1, ctxt2;
		ctxt1.push_back(IDM.getInstruction(6266));
		Value *v1 = dyn_cast<StoreInst>(IDM.getInstruction(4881))->getPointerOperand();
		Value *v2 = dyn_cast<LoadInst>(IDM.getInstruction(4508))->getPointerOperand();
		BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
		cerr << BAA.alias(&ctxt1, v1, 0, &ctxt2, v2, 0) << endl;
#endif
#if 0
		// Read the landmark trace. 
		read_trunks(TraceFile);
		// Read the clone mapping. 
		read_clone_map(CloneMapFile);
		// Calculate <sync_trunks> based on <trunks>.
		// A trunk may be bounded by non-enforcing landmarks. 
		// A sync trunk must be bounded by enforcing landmarks (except
		// the beginning and the end). 
		ThreadToTrunk sync_trunks;
		forall(ThreadToTrunk, it, trunks) {
			int thr_id = it->first;
			const vector<unsigned> &indices = it->second;
			size_t j = 0;
			while (j < indices.size()) {
				sync_trunks[thr_id].push_back(indices[j]);
				j = find_next_enforce(indices, j);
			}
			assert(sync_trunks[thr_id].size() > 0);
			if (sync_trunks[thr_id].back() < indices.back())
				sync_trunks[thr_id].push_back(indices.back());
		}
		// Collect racy pairs. 
		// Scan sync_trunks instead of trunks. 
		racy_pairs.clear();
		ThreadToTrunk::iterator i1, i2;
		for (i1 = sync_trunks.begin(); i1 != sync_trunks.end(); ++i1) {
			int t1 = i1->first;
			for (i2 = i1, ++i2; i2 != sync_trunks.end(); ++i2) {
				int t2 = i2->first;
				const vector<unsigned> &a1 = i1->second, &a2 = i2->second;
				// j1 and j2 are trunk IDs. 
				size_t j1 = 0;
				size_t j2 = 0;
				while (j1 < a1.size() && j2 < a2.size()) {
					// Thread t1 Trunk j1: [s1, e1)
					// Thread t2 Trunk j2: [s2, e2)
					// If the last trunk which has only one instruction, 
					// we virtually set e1 = s1 + 1 and e2 = s2 + 1.
					unsigned s1 = a1[j1];
					unsigned s2 = a2[j2];
					unsigned e1 = (j1 + 1 < a1.size() ? a1[j1 + 1] : s1 + 1);
					unsigned e2 = (j2 + 1 < a2.size() ? a2[j2 + 1] : s2 + 1);
					if (e1 > s2 && e2 > s1) {
#ifdef VERBOSE
						cerr << "Concurrent trunks: ";
						cerr << t1 << ":[" << s1 << ", " << e1 << ") "
							<< t2 << ":[" << s2 << ", " << e2 << ")" << endl;
#endif
						extract_racy_pairs(t1, s1, e1, t2, s2, e2);
					}
					if (j1 + 1 >= a1.size()) {
						++j2;
					} else if (j2 + 1 >= a2.size()) {
						++j1;
					} else {
						assert(j1 + 1 < a1.size() && j2 + 1 < a2.size());
						if (a1[j1 + 1] < a2[j2 + 1])
							++j1;
						else
							++j2;
					}
				}
			}
		}
#endif
		return false;
	}

	void RacyPairs::getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
		AU.addRequired<ObjectID>();
		AU.addRequired<BddAliasAnalysis>();
		AU.addRequired<TraceManager>();
#ifdef CONTEXT
		AU.addRequired<AddCallingContext>();
#endif
		ModulePass::getAnalysisUsage(AU);
	}

	char RacyPairs::ID = 0;
}

