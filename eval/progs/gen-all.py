#!/usr/bin/env python

import ConfigParser
import argparse
import sys, os

def invoke(cmd):
    print >> sys.stderr, cmd
    ret = os.system(cmd)
    if ret != 0:
        sys.exit(ret)

def work_on(config, section):
    assert config.has_section(section)
    id_bc = section + ".bc"
    simple_bc = section + ".simple.bc"
    invoke("../../scripts/slicer -f " + args.f + " " + section + " " + \
            id_bc + " " + simple_bc)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
            description = "Generate all programs to be evaluated")
    parser.add_argument("-f",
            help = "the configuration file (default: slicer.cfg)",
            default = "slicer.cfg")
    parser.add_argument("program",
            nargs = "*",
            help = "the program to run. should match the section name in the config file",
            default = "")
    args = parser.parse_args()

    assert os.path.exists(args.f)
    config = ConfigParser.ConfigParser()
    config.read(args.f)

    if len(args.program) == 0:
        for section in config.sections():
            # Dirty
            if section == "default" or section == "example":
                continue
            work_on(config, section)
    else:
        for section in args.program:
            work_on(config, section)
