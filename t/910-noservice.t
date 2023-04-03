#!/usr/bin/perl
# 
# Check that no service gives the correct message

use warnings;
use strict;
use Test::More;

defined($ARGV[0]) or die "Usage: $0 <top build dir>";

my $top_builddir = shift @ARGV;
my $nagios = "$top_builddir/base/nagios";
my $etc = "etc";
my $precache = "var/objects.precache";

plan tests => 1;

my $output = `$nagios -v "$etc/nagios-no-service.cfg"`;
like( $output, "/Error: There are no services defined!/", "Correct error for no services" );
