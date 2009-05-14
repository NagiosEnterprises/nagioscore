#!/usr/bin/perl
# 
# Checks nagiostats

use warnings;
use strict;
use Test::More;
use FindBin qw($Bin);

chdir $Bin or die "Cannot chdir";

my $topdir = "$Bin/..";
my $nagiostats = "$topdir/base/nagiostats";
my $etc = "$Bin/etc";

plan tests => 5;

my $output = `$nagiostats -c "$etc/nagios-does-not-exit.cfg"`;
isnt( $?, 0, "Bad return code with no config file" );
like( $output, "/Error processing config file/", "No config file" );

$output = `$nagiostats -c "$etc/nagios-no-status.cfg"`;
isnt( $?, 0, "Bad return code with no status file" );
like( $output, "/Error reading status file 'var/status.dat.no.such.file': No such file or directory/", "No config file" );

$output = `$nagiostats -c "$etc/nagios-no-status.cfg" -m NUMHSTUP`;
isnt( $?, 0, "Bad return code with no status file in MRTG mode" );

