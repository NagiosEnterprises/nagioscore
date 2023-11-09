#!/usr/bin/perl
# 
# Check that empty host/service groups pass verification.
# Likely error on non-patched version:
# "Error: Host 'r' specified in host group 'generic-pc' is not defined anywhere!"

use warnings;
use strict;
use Test::More;

defined($ARGV[0]) or die "Usage: $0 <top build dir>";

my $top_builddir = shift @ARGV;
my $nagios = "$top_builddir/base/nagios";
my $etc = "etc";

plan tests => 1;

my @output = `$nagios -v "$etc/nagios-empty-groups.cfg"`;
if ($? == 0) {
	pass("Nagios validated empty host/service-group successfully");
} else {
	@output = grep(/^Error: .+$/g, @output);
	fail("Nagios validation failed:\n@output");
}
