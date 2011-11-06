#!/usr/bin/env perl

while (<>) {
    if (/\[(.+)\]/) {
	next if $1 eq "default" or $1 eq "example";
	my $trace = `display-trace landmark < ../progs/$1.lt`;
	my $enforcing = 0;
	my $non_enforcing = 0;
	for (split /\n/, $trace) {
	    $enforcing = $1 if /# of occurences of enforcing landmarks = (\d+)/;
	    $non_enforcing = $1 if /# of occurences of non-enforcing landmarks = (\d+)/;
	}
	print "| $1 | $enforcing | $non_enforcing |\n";
    }
}
