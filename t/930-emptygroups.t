#!/usr/bin/perl
# 
# Check that empty host/service groups pass verfication.
# Likely error on non-patched version:
# "Error: Host 'r' specified in host group 'generic-pc' is not defined anywhere!"

use warnings;
use strict;
use Test::More;
use FindBin qw($Bin);

chdir $Bin or die "Cannot chdir";

my $topdir = "$Bin/..";
my $nagios = "$topdir/base/nagios";
my $etc = "$Bin/etc";

plan tests => 1;

my @output = `$nagios -v "$etc/nagios-empty-groups.cfg"`;
if ($? == 0) {
	pass("Nagios validated empty host/service-group successfully");
} else {
	@output = grep(/^Error: .+$/g, @output);
	fail("Nagios validation failed:\n@output");
}
