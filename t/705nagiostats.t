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
my $var = "$Bin/var";

plan tests => 10;

my $output = `$nagiostats -c "$etc/nagios-does-not-exist.cfg"`;
isnt( $?, 0, "Bad return code with no config file" );
like( $output, "/Error processing config file/", "No config file" );

$output = `$nagiostats -c "$etc/nagios-no-status.cfg"`;
isnt( $?, 0, "Bad return code with no status file" );
like( $output, "/Error reading status file '.*var/status.dat.no.such.file': No such file or directory/", "No config file" );

$output = `$nagiostats -c "$etc/nagios-no-status.cfg" -m -d NUMHSTUP`;
isnt( $?, 0, "Bad return code with no status file in MRTG mode" );
like( $output, "/^0\$/", "No UP host when no status file" );

$output = `$nagiostats -c "$etc/nagios-with-generated-status.cfg" -m -d NUMHOSTS`;
is( $?, 0, "Bad return code with implied status file in MRTG mode" );
unlike( $output, "/^0\$/", "Implied generated status file contains host(s)" );

$output = `$nagiostats -s "$var/status-generated.dat" -m -d NUMHOSTS`;
is( $?, 0, "Bad return code with explicit status file in MRTG mode" );
unlike( $output, "/^0\$/", "Explicit generated status file contains host(s)" );
