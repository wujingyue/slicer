#include "llvm/Support/CommandLine.h"

#include "config.h"
#include "racy-pairs.h"
#include "idm/id.h"
#include "bc2bdd/BddAliasAnalysis.h"
#include "common/include/util.h"

#include <fstream>
#include <sstream>

using namespace llvm;
using namespace repair;
using namespace std;

namespace {
	static RegisterPass<slicer::RacyPairs> X(
			"racy-pairs",
			"Dumps all racy pairs",
			false,
			true); // is analysis
	static cl::opt<string> TraceFile(
			"trace",
			cl::NotHidden,
			cl::desc("The landmark trace"),
			cl::init(""));
	static cl::opt<string> CloneMapFile(
			"clone-map",
			cl::NotHidden,
			cl::desc("The clone mapping"),
			cl::init(""));
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

	void RacyPairs::read_trace(const string &trace_file, Trace &trace) const {
		assert(trace_file.length() > 0 && "Didn't specify -trace");
		ifstream fin(trace_file.c_str());
		assert(fin && "Cannot open the specified landmark trace");
		int idx = 0;
		string line;
		while (getline(fin, line)) {
			istringstream iss(line);
			int thr_id;
			iss >> thr_id;
			trace[thr_id].push_back(idx);
			idx++;
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
			const InstMapping &m1,
			const InstMapping &m2) {
		BddAliasAnalysis &BAA = getAnalysis<BddAliasAnalysis>();
		forallconst(InstMapping, i1, m1) {
			Instruction *ins1 = (Cloned ? i1->second : i1->first);
			if (!isa<LoadInst>(ins1) && !isa<StoreInst>(ins1))
				continue;
			forallconst(InstMapping, i2, m2) {
				Instruction *ins2 = (Cloned ? i2->second : i2->first);
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
				if (BAA.alias(v1, 0, v2, 0))
					racy_pairs.push_back(make_pair(ins1, ins2));
			}
		}
	}

	bool RacyPairs::runOnModule(Module &M) {
		// Read the landmark trace. 
		Trace trace;
		read_trace(TraceFile, trace);
		// Read the clone mapping. 
		map<int, vector<InstMapping> > clone_map;
		read_clone_map(CloneMapFile, clone_map);
		// Collect racy pairs. 
		racy_pairs.clear();
		cerr << trace.size() << endl;
		for (Trace::iterator i1 = trace.begin(); i1 != trace.end(); ++i1) {
			int t1 = i1->first;
			Trace::iterator i2 = i1; ++i2;
			for (; i2 != trace.end(); ++i2) {
				int t2 = i2->first;
				const vector<int> &a1 = i1->second, &a2 = i2->second;
				// j1 and j2 are trunk IDs. 
				size_t j1 = 0, j2 = 0;
				while (j1 < a1.size() && j2 < a2.size()) {
					// Thread t1 Trunk j1: [s1, e1)
					// Thread t2 Trunk j2: [s2, e2)
					// If the last trunk which has only one instruction, 
					// we virtually set e1 = s1 + 1 and e2 = s2 + 1.
					int s1 = a1[j1], s2 = a2[j2];
					int e1 = (j1 + 1 < a1.size() ? a1[j1 + 1] : s1 + 1);
					int e2 = (j2 + 1 < a2.size() ? a2[j2 + 1] : s2 + 1);
					if (e1 > s2 && e2 > s1) {
#ifdef VERBOSE
						cerr << "Concurrent trunks: ";
						cerr << t1 << ":[" << s1 << ", " << e1 << ") "
							<< t2 << ":[" << s2 << ", " << e2 << ")" << endl;
#endif
						// Compare trunk j1 in thread t1 and trunk j2 in thread t2. 
						extract_racy_pairs(clone_map[t1][j1], clone_map[t2][j2]);
					}
					if (j1 + 1 == a1.size())
						++j2;
					else if (j2 + 1 == a2.size())
						++j1;
					else {
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
		ModulePass::getAnalysisUsage(AU);
	}

	char RacyPairs::ID = 0;
}

