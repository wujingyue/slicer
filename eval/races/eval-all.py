#!/usr/bin/env python

import os, sys, string
import ConfigParser

def invoke(cmd):
    print >> sys.stderr, '\033[1;34m' + cmd + '\033[m'
    ret = os.system(cmd)
    if ret != 0:
        sys.exit(ret)

if __name__ == '__main__':
    config = ConfigParser.ConfigParser({'simplify': '0',
                                        'simplifier-args': '',
                                        'cs': '0',
                                        'concurrent': '0',
                                        'adv-aa': '0',
                                        'sample': '0',
                                        'args': ''})
    config.read('config.ini')
    benchmarks = config.sections()
    if 1 < len(sys.argv):
        specified_benchmark = sys.argv[1]
    else:
        specified_benchmark = ''

    LLVM_ROOT = os.getenv('LLVM_ROOT')
    SLICER_ROOT = os.getenv('SLICER_ROOT')
    SCRIPT_DIR = os.path.join(SLICER_ROOT, 'scripts')
    PROGS_DIR = os.path.join(SLICER_ROOT, 'eval/progs')

    for benchmark in benchmarks:
        # Skip unspecified benchmarks
        if specified_benchmark != '' and specified_benchmark != benchmark:
            continue

        option_simplify = config.getboolean(benchmark, 'simplify')
        option_simplifier_args = config.get(benchmark, 'simplifier-args')
        option_cs = config.getboolean(benchmark, 'cs')
        option_concurrent = config.getboolean(benchmark, 'concurrent')
        option_adv_aa = config.getboolean(benchmark, 'adv-aa')
        option_sample = config.getint(benchmark, 'sample')
        option_args = config.get(benchmark, 'args')

        # print some rubbish
        print >> sys.stderr, 'Running benchmark', benchmark

        # prepare: .bc -> .prep.bc
        base_cmd = os.path.join(SCRIPT_DIR, 'prepare')
        input_filename = os.path.join(PROGS_DIR, benchmark + '.bc')
        output_filename = os.path.join(PROGS_DIR, benchmark + '.prep.bc')
        cmd = string.join((base_cmd, input_filename, output_filename))
        invoke(cmd)

        # id-manager: .prep.bc -> .id.bc
        base_cmd = os.path.join(SCRIPT_DIR, 'tag-id')
        input_filename = os.path.join(PROGS_DIR, benchmark + '.prep.bc')
        output_filename = os.path.join(PROGS_DIR, benchmark + '.id.bc')
        cmd = string.join((base_cmd, input_filename, output_filename))
        invoke(cmd)

        # trace: .id.bc -> .bc1 -> .trace.bc -> .trace.s -> .trace
        base_cmd = os.path.join(SCRIPT_DIR, 'add-tracing-code')
        input_filename = os.path.join(PROGS_DIR, benchmark)
        cmd = string.join((base_cmd, input_filename))
        invoke(cmd)

        # generate full trace (1/2): .trace -> /tmp/fulltrace
        base_cmd = os.path.join(PROGS_DIR, benchmark + '.trace')
        cmd = string.join((base_cmd, option_args))
        invoke(cmd)

        # generate full trace (2/2): /tmp/fulltrace -> .ft
        base_cmd = 'mv'
        input_filename = '/tmp/fulltrace'
        output_filename = os.path.join(PROGS_DIR, benchmark + '.ft')
        cmd = string.join((base_cmd, input_filename, output_filename))
        invoke(cmd)

        # generate landmark trace: .ft -> .lt
        base_cmd = os.path.join(SCRIPT_DIR, 'gen-landmark-trace')
        input_filename = os.path.join(PROGS_DIR, benchmark + '.ft')
        output_filename = os.path.join(PROGS_DIR, benchmark + '.lt')
        cmd = string.join((base_cmd, input_filename, output_filename))
        invoke(cmd)

        # max-slicing: .id.bc -> .slice.bc
        base_cmd = os.path.join(SCRIPT_DIR, 'max-slicing')
        input_filename = os.path.join(PROGS_DIR, benchmark + '.id.bc')
        output_filename = os.path.join(PROGS_DIR, benchmark + '.slice.bc')
        cmd = string.join((base_cmd, input_filename, output_filename))
        invoke(cmd)

        # simplify: .slice.bc -> .simple.bc
        filename_suffix = '.slice'
        if option_simplify:
            base_cmd = os.path.join(SCRIPT_DIR, 'simplify')
            input_filename = os.path.join(PROGS_DIR, benchmark + '.slice.bc')
            output_filename = os.path.join(PROGS_DIR, benchmark + '.simple.bc')
            cmd = string.join((base_cmd, input_filename, output_filename, option_simplifier_args))
            invoke(cmd)
            filename_suffix = '.simple'

        # generate queries: -> .id.queries, .slice(simple).raw_queries
        base_cmd = 'opt '
        base_cmd += '-load ' + LLVM_ROOT + '/install/lib/libid-manager.so '
        base_cmd += '-load ' + LLVM_ROOT + '/install/lib/libbc2bdd.so '
        base_cmd += '-load ' + LLVM_ROOT + '/install/lib/libcallgraph-fp.so '
        base_cmd += '-load ' + LLVM_ROOT + '/install/lib/libmbb.so '
        base_cmd += '-load ' + LLVM_ROOT + '/install/lib/libcfg.so '
        base_cmd += '-load ' + LLVM_ROOT + '/install/lib/libslicer-trace.so '
        base_cmd += '-load ' + LLVM_ROOT + '/install/lib/libmax-slicing.so '
        base_cmd += '-load ' + LLVM_ROOT + '/install/lib/libint.so '
        base_cmd += '-load ' + LLVM_ROOT + '/install/lib/libalias-query.so '
        cmd_options = '-analyze '
        if option_cs:
            cmd_options += '-cs '
        if not option_concurrent:
            # static queries
            input_filename = os.path.join(PROGS_DIR, benchmark + filename_suffix + '.bc')
        else:
            input_filename = os.path.join(PROGS_DIR, benchmark + '.id.bc')
            full_trace_filename = os.path.join(PROGS_DIR, benchmark + '.ft')
            landmark_trace_filename = os.path.join(PROGS_DIR, benchmark + '.lt')
            cmd_options += '-fulltrace ' + full_trace_filename + ' '
            cmd_options += '-input-landmark-trace ' + landmark_trace_filename + ' '
            cmd_options += '-concurrent '
        cmd_options += '-gen-queries '
        output_filename = os.path.join(PROGS_DIR, benchmark + '.id.queries')
        cmd = string.join((base_cmd, cmd_options, '-for-orig',
                           '<', input_filename, '>', output_filename))
        invoke(cmd)
        output_filename = os.path.join(PROGS_DIR, benchmark + filename_suffix + '.raw_queries')
        cmd = string.join((base_cmd, cmd_options,
                           '<', input_filename, '>', output_filename))
        invoke(cmd)

        # translate queries: .slice(simple).raw_queries -> .slice(simple).queries
        cmd_options = '-disable-output -translate-queries '
        bc_filename = os.path.join(PROGS_DIR, benchmark + filename_suffix + '.bc')
        input_filename = os.path.join(PROGS_DIR, benchmark + filename_suffix + '.raw_queries')
        output_filename = os.path.join(PROGS_DIR, benchmark + filename_suffix + '.queries')
        cmd = string.join((base_cmd, cmd_options,
                           '-input-raw-queries', input_filename,
                           '-output-queries', output_filename,
                           '<', bc_filename))
        invoke(cmd)

        # drive queries: .id.queries, .slice(simple).queries ->
        cmd_options = '-analyze '
        landmark_trace_filename = os.path.join(PROGS_DIR, benchmark + '.lt')
        if option_sample > 1:
            cmd_options += '-sample ' + str(option_sample) + ' '
        cmd_options += '-drive-queries '
        bc_filename = os.path.join(PROGS_DIR, benchmark + '.id.bc')
        input_filename = os.path.join(PROGS_DIR, benchmark + '.id.queries')
        cmd = string.join((base_cmd, cmd_options,
                           '-query-list', input_filename,
                           '<', bc_filename))
        invoke(cmd)
        if option_adv_aa:
            cmd_options += '-use-adv-aa '
            cmd_options += '-input-landmark-trace ' + landmark_trace_filename + ' '
        bc_filename = os.path.join(PROGS_DIR, benchmark + filename_suffix + '.bc')
        input_filename = os.path.join(PROGS_DIR, benchmark + filename_suffix + '.queries')
        cmd = string.join((base_cmd, cmd_options,
                           '-query-list', input_filename, '-debug-only=alias-query',
                           '<', bc_filename))
        invoke(cmd)
