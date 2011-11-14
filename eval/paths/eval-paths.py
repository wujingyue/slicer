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
            description = "Count the number of paths w.r.t. the original bc and the simplified bc")
    parser.add_argument("-f",
            help = "the configration file (default: slicer.cfg)",
            default = "slicer.cfg")
    parser.add_argument("program",
            nargs = "?",
            help = "the name of the program, used as the section name",
            default = "")
    args = parser.parse_args()

    assert os.path.exists(args.f)
    config = ConfigParser.ConfigParser({"iter": "4"})
    config.read(args.f)

    LLVM_ROOT = os.getenv("LLVM_ROOT")
    SLICER_ROOT = os.getenv("SLICER_ROOT")
    PROGS_DIR = os.path.join(SLICER_ROOT, "eval/progs")

    for section in config.sections():
        # Dirty
        if section == "default" or section == "example":
            continue
        # Skip unspecified sections
        if args.program != "" and args.program != section:
            continue

        main_file_name = os.path.join(PROGS_DIR, section)
        input_bc = main_file_name + ".bc"
        simple_bc = main_file_name + ".simple.bc"

        base_cmd = "opt " + \
                "-load $LLVM_ROOT/install/lib/id-manager.so " + \
                "-load $LLVM_ROOT/install/lib/bc2bdd.so " + \
                "-load $LLVM_ROOT/install/lib/callgraph-fp.so " + \
                "-load $LLVM_ROOT/install/lib/mbb.so " + \
                "-load $LLVM_ROOT/install/lib/cfg.so " + \
                "-load $LLVM_ROOT/install/lib/slicer-trace.so " + \
                "-load $LLVM_ROOT/install/lib/max-slicing.so " + \
                "-load $LLVM_ROOT/install/lib/int.so " + \
                "-load $LLVM_ROOT/install/lib/metrics.so "
        cmd_options = "-analyze -count-paths "
        cmd_options += "-iter " + config.get(section, "iter")
        cmd = string.join((base_cmd, cmd_options, "<", input_bc))
        invoke(cmd)
        cmd = string.join((base_cmd, cmd_options, "<", simple_bc))
        invoke(cmd)
