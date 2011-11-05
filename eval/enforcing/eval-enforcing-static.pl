#!/usr/bin/env perl

my $LLVM_ROOT = $ENV{"LLVM_ROOT"};

while (<>) {
    if (/\[(.+)\]/) {
	next if $1 eq "default" or $1 eq "example";
	my $stat = `opt -load $LLVM_ROOT/install/lib/libid-manager.so -load $LLVM_ROOT/install/lib/libbc2bdd.so -load $LLVM_ROOT/install/lib/libcallgraph-fp.so -load $LLVM_ROOT/install/lib/libmbb.so -load $LLVM_ROOT/install/lib/libcfg.so -load $LLVM_ROOT/install/lib/libpreparer.so -load $LLVM_ROOT/install/lib/libslicer-trace.so -load $LLVM_ROOT/install/lib/libmax-slicing.so -load $LLVM_ROOT/install/lib/libint.so -mark-landmarks -stats -disable-output < ../progs/$1.id.bc 2>&1`;
	my $enforcing = 0;
	my $derived = 0;
	for (split /\n/, $stat) {
	    $enforcing = $1 if /(\d+) trace - Number of enforcing landmarks/;
	    $derived = $1 if /(\d+) trace - Number of derived landmarks/;
	}
	print "| $1 | $enforcing | $derived |\n";
    }
}
