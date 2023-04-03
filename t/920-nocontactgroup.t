#!/usr/bin/perl
# 
# Check that no contactgroup gives the correct message

use warnings;
use strict;
use Test::More qw(no_plan);

defined($ARGV[0]) or die "Usage: $0 <top build dir>";

my $top_builddir = shift @ARGV;
my $nagios = "$top_builddir/base/nagios";
my $etc = "etc";
my $precache = "var/objects.precache";


my $output = `$nagios -v "$etc/nagios-no-contactgroup.cfg"`;
like( $output, "/Error: Could not find any contactgroup matching 'nonexistentone'/", "Correct error for no contactgroup" );
isnt($?, 0, "And get return code error" );
