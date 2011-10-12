import os

def get_base_cmd():
    return "opt " + \
            "-load $LLVM_ROOT/install/lib/libid-manager.so " + \
            "-load $LLVM_ROOT/install/lib/libbc2bdd.so " + \
            "-load $LLVM_ROOT/install/lib/libcallgraph-fp.so " + \
            "-load $LLVM_ROOT/install/lib/libmbb.so " + \
            "-load $LLVM_ROOT/install/lib/libcfg.so " + \
            "-load $LLVM_ROOT/install/lib/libprepare.so " + \
            "-load $LLVM_ROOT/install/lib/libslicer-trace.so " + \
            "-load $LLVM_ROOT/install/lib/libmax-slicing.so " + \
            "-load $LLVM_ROOT/install/lib/libint.so "

def invoke(cmd):
    # Print the command in blue.
    print >> sys.stderr, '\033[1;34m' + cmd + '\033[m'
    ret = os.system(cmd)
    if ret != 0:
        sys.exit(ret)

def prepare(bc, prep_bc, customized_thread_funcs):
    cmd = get_base_cmd()
    cmd += "-prepare "
    for customized_thread_func in customized_thread_funcs:
        cmd += "-thread-func " + customized_thread_func + " "
    cmd += "-o " + prep_bc + " < " + bc
    invoke(cmd)

def tag_id(prep_bc, id_bc):
    cmd = get_base_cmd()
    cmd += "-tag-id "
    cmd += "-o " + id_bc + " < " + prep_bc
    invoke(cmd)

def instrument(id_bc, trace_bc, customized_enforcing_landmarks,
        instrument_each_bb, multi_processed):

