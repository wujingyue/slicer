#!/usr/bin/env python

import os
import sys
import string
import ConfigParser
import argparse

def invoke(cmd):
    print >> sys.stderr, cmd
    ret = os.system(cmd)
    if ret != 0:
        sys.exit(ret)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
            description = "Evaluate races")
    parser.add_argument("-f",
            help = "the configration file (default: slicer.cfg)",
            default = "slicer.cfg")
    parser.add_argument("program",
            nargs = "?",
            help = "the name of the program, used as the section name",
            default = "")
    args = parser.parse_args()

    assert os.path.exists(args.f)
    config = ConfigParser.ConfigParser({"cs": "0",
                                        "concurrent": "0",
                                        "adv-aa": "0",
                                        "sample": "0"})
    config.read(args.f)
    benchmarks = config.sections()
    specified_benchmark = args.program

    LLVM_ROOT = os.getenv("LLVM_ROOT")
    SLICER_ROOT = os.getenv("SLICER_ROOT")
    PROGS_DIR = os.path.join(SLICER_ROOT, "eval/progs")

    for benchmark in benchmarks:
        # Dirty
        if benchmark == "default" or benchmark == "example":
            continue
        # Skip unspecified benchmarks
        if specified_benchmark != "" and specified_benchmark != benchmark:
            continue

        option_cs = config.getboolean(benchmark, "cs")
        option_concurrent = config.getboolean(benchmark, "concurrent")
        option_adv_aa = config.getboolean(benchmark, "adv-aa")
        option_sample = config.getint(benchmark, "sample")

        # print some rubbish
        print >> sys.stderr, "Running benchmark", benchmark

        # generate queries: -> .id.queries, .simple.raw_queries
        base_cmd = "opt " + \
                "-load $LLVM_ROOT/install/lib/libid-manager.so " + \
                "-load $LLVM_ROOT/install/lib/libbc2bdd.so " + \
                "-load $LLVM_ROOT/install/lib/libcallgraph-fp.so " + \
                "-load $LLVM_ROOT/install/lib/libmbb.so " + \
                "-load $LLVM_ROOT/install/lib/libcfg.so " + \
                "-load $LLVM_ROOT/install/lib/libslicer-trace.so " + \
                "-load $LLVM_ROOT/install/lib/libmax-slicing.so " + \
                "-load $LLVM_ROOT/install/lib/libint.so " + \
                "-load $LLVM_ROOT/install/lib/libalias-query.so "
        cmd_options = "-analyze "
        if option_cs:
            cmd_options += "-cs "
        if not option_concurrent:
            # static queries
            input_filename = os.path.join(PROGS_DIR, benchmark + ".simple.bc")
        else:
            input_filename = os.path.join(PROGS_DIR, benchmark + ".id.bc")
            full_trace_filename = os.path.join(PROGS_DIR, benchmark + ".ft")
            landmark_trace_filename = os.path.join(PROGS_DIR, benchmark + ".lt")
            cmd_options += "-fulltrace " + full_trace_filename + " "
            cmd_options += "-input-landmark-trace " + landmark_trace_filename + " "
            cmd_options += "-concurrent "
        cmd_options += "-gen-queries "
        output_filename = os.path.join(PROGS_DIR, benchmark + ".id.queries")
        cmd = string.join((base_cmd, cmd_options, "-for-orig",
                           "<", input_filename, ">", output_filename))
        invoke(cmd)
        output_filename = os.path.join(PROGS_DIR, benchmark + ".simple.raw_queries")
        cmd = string.join((base_cmd, cmd_options,
                           "<", input_filename, ">", output_filename))
        invoke(cmd)

        # translate queries: .simple.raw_queries -> .simple.queries
        cmd_options = "-disable-output -translate-queries "
        bc_filename = os.path.join(PROGS_DIR, benchmark + ".simple.bc")
        input_filename = os.path.join(PROGS_DIR, benchmark + ".simple.raw_queries")
        output_filename = os.path.join(PROGS_DIR, benchmark + ".simple.queries")
        cmd = string.join((base_cmd, cmd_options,
                           "-input-raw-queries", input_filename,
                           "-output-queries", output_filename,
                           "<", bc_filename))
        invoke(cmd)

        # drive queries: .id.queries, .simple.queries ->
        cmd_options = "-analyze "
        landmark_trace_filename = os.path.join(PROGS_DIR, benchmark + ".lt")
        if option_sample > 1:
            cmd_options += "-sample " + str(option_sample) + " "
        cmd_options += "-drive-queries "
        bc_filename = os.path.join(PROGS_DIR, benchmark + ".id.bc")
        input_filename = os.path.join(PROGS_DIR, benchmark + ".id.queries")
        cmd = string.join((base_cmd, cmd_options,
                           "-query-list", input_filename,
                           "<", bc_filename))
        invoke(cmd)
        if option_adv_aa:
            cmd_options += "-use-adv-aa "
            cmd_options += "-input-landmark-trace " + landmark_trace_filename + " "
        bc_filename = os.path.join(PROGS_DIR, benchmark + ".simple.bc")
        input_filename = os.path.join(PROGS_DIR, benchmark + ".simple.queries")
        cmd = string.join((base_cmd, cmd_options,
                           "-query-list", input_filename,
                           "<", bc_filename))
        invoke(cmd)
