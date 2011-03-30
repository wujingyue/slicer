#include "llvm/Support/CommandLine.h"

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
}

namespace slicer {

	void RacyPairs::print_inst(Instruction *ins, raw_ostream &O) const {
		BasicBlock *bb = ins->getParent();
		Function *func = bb->getParent();
		O << func->getNameStr() << ":" << bb->getNameStr() << ":\t";
		ins->print(O);
		O << "\n";
	}

	void RacyPairs::print(raw_ostream &O, const Module *M) const {
		forallconst(vector<InstPair>, it, racy_pairs) {
			O << string(10, '=') << "\n";
			print_inst(it->first, O);
			print_inst(it->second, O);
		}
	}

	void RacyPairs::read_trunks(
			const string &trace_file,
			ThreadToTrunk &trunks) const {
		ifstream fin(trace_file.c_str());
		assert(fin && "Cannot open the specified landmark trace");
		string line;
		while (getline(fin, line)) {
			istringstream iss(line);
			int idx;
			iss >> idx;
			TraceManager &TM = getAnalysis<TraceManager>();
			trunks[TM.get_record(idx).thr_id].push_back(idx);
		}
	}

	void RacyPairs::read_clone_map(
			const string &clone_map_file,
			map<int, vector<InstMapping> > &clone_map) const {
		assert(clone_map_file.length() > 0 && "Didn't specify -clone-map");
		ifstream fin(clone_map_file.c_str());
		assert(fin && "Cannot open the specified clone map file");
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
				clone_map[thr_id][trunk_id][orig] = cloned;
			}
		}
	}

	void RacyPairs::extract_racy_pairs(
			int t1, unsigned s1, unsigned e1,
			int t2, unsigned s2, unsigned e2,
			const ThreadToTrunk &trunks,
			const map<int, vector<InstMapping> > &clone_map) {

		BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
		TraceManager &TM = getAnalysis<TraceManager>();
		AddCallingContext &ACC = getAnalysis<AddCallingContext>();
		
		const vector<unsigned> &a1 = trunks.find(t1)->second;
		const vector<unsigned> &a2 = trunks.find(t2)->second;
		// p1 and p2 are global indices. 
		// TODO: Can be optimized. a1 and a2 are ordered. 
		for (unsigned p1 = s1; p1 < e1; ++p1) {
			size_t tr1 = (upper_bound(a1.begin(), a1.end(), p1) - a1.begin()) - 1;
			Instruction *ins1 = TM.get_record(p1).ins;
			if (Cloned) {
				Instruction *c = clone_map.find(t1)->second[tr1].lookup(ins1);
				if (c)
					ins1 = c;
			}
			if (!isa<LoadInst>(ins1) && !isa<StoreInst>(ins1))
				continue;
			for (unsigned p2 = s2; p2 < e2; ++p2) {
				size_t tr2 = (upper_bound(a2.begin(), a2.end(), p2) - a2.begin()) - 1;
				Instruction *ins2 = TM.get_record(p2).ins;
				if (Cloned) {
					Instruction *c = clone_map.find(t2)->second[tr2].lookup(ins2);
					if (c)
						ins2 = c;
				}
				if (!isa<LoadInst>(ins2) && !isa<StoreInst>(ins2))
					continue;
				// If both are reads, not a race. 
				if (isa<LoadInst>(ins1) && isa<LoadInst>(ins2))
					continue;
				Value *v1 = (isa<LoadInst>(ins1) ?
						dyn_cast<LoadInst>(ins1)->getPointerOperand() :
						dyn_cast<StoreInst>(ins1)->getPointerOperand());
				Value *v2 = (isa<LoadInst>(ins2) ?
						dyn_cast<LoadInst>(ins2)->getPointerOperand() :
						dyn_cast<StoreInst>(ins2)->getPointerOperand());
				vector<CallInst *> ctxt1, ctxt2;
				ctxt1 = convert_context(ACC.get_calling_context(p1));
				ctxt2 = convert_context(ACC.get_calling_context(p2));
				if (BAA.alias(&ctxt1, v1, 0, &ctxt2, v2, 0))
					racy_pairs.push_back(make_pair(ins1, ins2));
			}
		}
	}

	vector<CallInst *> RacyPairs::convert_context(const CallStack &cs) {
		vector<CallInst *> res;
		for (size_t i = 0, E = cs.size(); i < E; ++i) {
			assert(isa<CallInst>(cs[i]));
			res.push_back(dyn_cast<CallInst>(cs[i]));
		}
		return res;
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
		// Read the landmark trace. 
		ThreadToTrunk trunks;
		read_trunks(TraceFile, trunks);
		// Read the clone mapping. 
		map<int, vector<InstMapping> > clone_map;
		read_clone_map(CloneMapFile, clone_map);
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
					unsigned e2 = (j1 + 1 < a2.size() ? a2[j2 + 1] : s2 + 1);
					if (e1 > s2 && e2 > s1) {
#ifdef VERBOSE
						cerr << "Concurrent trunks: ";
						cerr << t1 << ":[" << s1 << ", " << e1 << ") "
							<< t2 << ":[" << s2 << ", " << e2 << ")" << endl;
#endif
						extract_racy_pairs(t1, s1, e1, t2, s2, e2, trunks, clone_map);
					}
					if (j1 + 1 < a1.size()) {
						++j2;
					} else if (j2 + 1 < a2.size()) {
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
		return false;
	}

	void RacyPairs::getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
		AU.addRequired<ObjectID>();
		AU.addRequired<BddAliasAnalysis>();
		AU.addRequired<TraceManager>();
		ModulePass::getAnalysisUsage(AU);
	}

	char RacyPairs::ID = 0;
}

