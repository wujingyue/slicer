#!/usr/bin/env perl

my $LLVM_ROOT = $ENV{"LLVM_ROOT"};

while (<>) {
    if (/\[(.+)\]/) {
	next if $1 eq "default" or $1 eq "example";
	my $stat = `opt -load $LLVM_ROOT/install/lib/id-manager.so -load $LLVM_ROOT/install/lib/bc2bdd.so -load $LLVM_ROOT/install/lib/callgraph-fp.so -load $LLVM_ROOT/install/lib/mbb.so -load $LLVM_ROOT/install/lib/cfg.so -load $LLVM_ROOT/install/lib/preparer.so -load $LLVM_ROOT/install/lib/slicer-trace.so -load $LLVM_ROOT/install/lib/max-slicing.so -load $LLVM_ROOT/install/lib/int.so -mark-landmarks -stats -disable-output < ../progs/$1.id.bc 2>&1`;
	my $enforcing = 0;
	my $derived = 0;
	for (split /\n/, $stat) {
	    $enforcing = $1 if /(\d+) trace - Number of enforcing landmarks/;
	    $derived = $1 if /(\d+) trace - Number of derived landmarks/;
	}
	print "| $1 | $enforcing | $derived |\n";
    }
}
