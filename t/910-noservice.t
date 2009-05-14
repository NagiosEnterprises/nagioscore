#!/usr/bin/perl
# 
# Check that no service gives the correct message

use warnings;
use strict;
use Test::More;
use FindBin qw($Bin);

chdir $Bin or die "Cannot chdir";

my $topdir = "$Bin/..";
my $nagios = "$topdir/base/nagios";
my $etc = "$Bin/etc";
my $precache = "$Bin/var/objects.precache";

plan tests => 1;

my $output = `$nagios -v "$etc/nagios-no-service.cfg"`;
like( $output, "/Error: There are no services defined!/", "Correct error for no services" );
